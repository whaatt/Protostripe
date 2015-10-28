/**
 * File: sequencer.cpp
 * Author: Sanjay Kannan
 * ---------------------
 * Creates a FluidSynth sequencer that allows
 * for arbitrary scheduling of note layers.
 */

#include "sequencer.h"
#include "layer.h"
using namespace std;

/**
 * Constructor: Sequencer
 * ------------------------
 * Sets FluidSynth object to NULL.
 */
Sequencer::Sequencer()
  : sequencer(NULL) {}

/**
 * Destructor: Sequencer
 * ---------------------
 * Cleans up FluidSynth object.
 */
Sequencer::~Sequencer() {
  // lock sequencer
  seqLock.lock();

  // this ugly iterator syntax is unnecesary in C++11, but OpenFrameworks is annoying
  for (itType iterator = channels.begin(); iterator != channels.end(); iterator++)
    fluid -> allNotesOff(iterator -> first); // avoid shadow notes

  // clean up FluidSynth sequencer object
  if (sequencer) delete_fluid_sequencer(sequencer);
  cerr << "Deleting sequencer object." << endl;
  sequencer = NULL;

  // unlock sequencer
  seqLock.unlock();
}

/**
 * Function: init
 * --------------
 * Sets sequencer synth and
 * sequencer running tempo.
 */
bool Sequencer::init(Synthesizer* synth, int beatsMinute, NoteHandler call, void* data) {
  // synth sanity check
  if (synth -> synth == NULL) {
    // do not attach sequencer to invalid synth object
    cerr << "Synthesizer not initialized." << endl;
    return false;
  }

  // null sanity check
  if (sequencer != NULL) {
    // check if sequencer has already been initialized
    cerr << "Sequencer already initialized." << endl;
    return false;
  }

  // lock sequencer
  seqLock.lock();

  // TODO: add a non-verbose mode to constructor
  cerr << "Initializing sequencer object." << endl;

  beatsPerMinute = beatsMinute;
  msPerBeat = 60000 / beatsPerMinute;

  // initialize the sequencer itself
  sequencer = new_fluid_sequencer();

  // lock synth
  fluid = synth;
  fluid -> synthLock.lock();

  // register the synth with the sequencer [or is it the other way around]
  synthSeqID = fluid_sequencer_register_fluidsynth(sequencer, fluid -> synth);
  mySeqID = fluid_sequencer_register_client(sequencer, "this", &Sequencer::callback, this);

  // unlock synth
  fluid -> synthLock.unlock();

  now = fluid_sequencer_get_tick(sequencer);
  globalBeatCount = -1; // start in advance
  handler = call; // register note handler
  callData = data; // with custom data
  scheduleLayers(); // schedule note layers

  // unlock sequencer
  seqLock.unlock();
  return sequencer != NULL;
}

/**
 * Function: scheduleLayers
 * ------------------------
 * Schedules all layers before a beat. Notes
 * are very lazily scheduled at the beat in
 * which they first appear.
 */
void Sequencer::scheduleLayers() {
  // staggering half beat behind
  now = now + msPerBeat / 2;
  globalBeatCount += 1;

  // useful logging code if callbacks are failing:
  // cout << "Beat to occur at " << now << "." << endl;

  // this ugly iterator syntax is unnecesary in C++11, but OpenFrameworks is annoying
  for (itType iterator = channels.begin(); iterator != channels.end(); iterator++) {
    Layer* current = &channels[iterator -> first];
    int channel = current -> channel;
    int beatStart = current -> beatStart;
    int beatCount = current -> beatCount;

    // skip muted layers
    if (current -> muted)
      continue;

    // junk layer created
    if (beatCount == 0)
      continue;

    // remember when we
    // started this layer
    if (beatStart == -1) // never forget
      current -> beatStart = globalBeatCount;

    // see which measure of the layer we are on and calculate time offset
    int beatPos = (globalBeatCount - current -> beatStart) % beatCount;
    int beatPosDiff = msPerBeat * beatPos;

    // advance schedule all of the notes in layer
    for (int i = 0; i < current -> notes.size(); i += 1) {
      Note note = current -> notes[i]; // iterate through each note in vector
      if (note.msOffset < beatPosDiff || note.msOffset >= beatPosDiff + msPerBeat)
        continue; // continue if the note has been or is not ready to be scheduled
      sendNoteOn(channel, note.pitch, note.velocity, now + note.msOffset - beatPosDiff);
      sendNoteOff(channel, note.pitch, now + note.msOffset - beatPosDiff + note.msDuration);

      // notify graphics handler of notes in layer on demand like audio
      int distFromRealNow = note.msOffset - beatPosDiff + msPerBeat / 2;
      handler(callData, channel, note.position, note.velocity, distFromRealNow, note.msDuration);
    }
  }

  // see below
  scheduleTimer();
}

/**
 * Function: scheduleTimer
 * -----------------------
 * Schedules timer when next
 * beat comes around.
 */
void Sequencer::scheduleTimer() {
  // set timer at the stagger point
  now = now + msPerBeat / 2; // half
  seqLock.lock();

  // create timer event and schedule it
  fluid_event_t* event = new_fluid_event();
  fluid_event_set_source(event, -1);
  fluid_event_set_dest(event, mySeqID);
  fluid_event_timer(event, NULL);
  // cout << "Timer to occur at " << now << "." << endl;
  fluid_sequencer_send_at(sequencer, event, now, 1);

  // clean up event
  delete_fluid_event(event);
  seqLock.unlock();
}

/**
 * Function: writeLayer
 * --------------------
 * Adds a sequence to be played on a given
 * channel. The sequence starts at the next
 * beat tick and will be played periodically.
 */
void Sequencer::writeLayer(int channel, Layer layer) {
  // right now the entire layer is copied. in the
  // future, we might want to allocate memory for
  // layers and pass by reference here
  channels[channel] = layer;
}

/**
 * Function: toggleLayerIfExists
 * -----------------------------
 * Toggle the muting on a given layer
 * if it already exists in the map.
 */
void Sequencer::toggleLayerIfExists(int channel) {
  if (channels.count(channel) == 0) return;
  bool oldState = channels[channel].muted;
  channels[channel].muted = !oldState;

  // now muting so reset the channel reference point
  if (!oldState) channels[channel].beatStart = -1;
  else channels[channel].beatStart = globalBeatCount;
  fluid -> allNotesOff(channel);
}

/**
 * Function: getGlobalBeatCount
 * ----------------------------
 * Get the number of beats since
 * the sequencer was initialized.
 */
int Sequencer::getGlobalBeatCount() {
  // just an accessor because style
  return globalBeatCount;
}

/**
 * Static Function: callback
 * -------------------------
 * Calls when timer goes off for beat.
 */
void Sequencer::callback(unsigned int time, fluid_event_t* event, fluid_sequencer_t* seq, void* data) {
  // this will advance the schedule-note-schedule-timer cycle
  Sequencer* current = (Sequencer*) data; // data was passed as this
  // cout << "Called back at " << current -> now << "." << endl;
  current -> scheduleLayers();
}

/**
 * Function: sendNoteOn
 * --------------------
 * Schedules a note on a channel.
 */
void Sequencer::sendNoteOn(int channel, short key, short velocity, unsigned int date) {
  int fluidSched;
  seqLock.lock();

  // create note event and schedule it
  fluid_event_t* event = new_fluid_event();
  fluid_event_set_source(event, -1);
  fluid_event_set_dest(event, synthSeqID);
  fluid_event_noteon(event, channel, key, velocity);
  fluidSched = fluid_sequencer_send_at(sequencer, event, date, 1);

  // clean up event
  seqLock.unlock();
  delete_fluid_event(event);
}

/**
 * Function: sendNoteOff
 * ---------------------
 * Schedules a note off on a channel.
 */
void Sequencer::sendNoteOff(int channel, short key, unsigned int date) {
  int fluidSched;
  seqLock.lock();

  // create note event and schedule it
  fluid_event_t* event = new_fluid_event();
  fluid_event_set_source(event, -1);
  fluid_event_set_dest(event, synthSeqID);
  fluid_event_noteoff(event, channel, key);
  fluidSched = fluid_sequencer_send_at(sequencer, event, date, 1);

  // clean up event
  seqLock.unlock();
  delete_fluid_event(event);
}
