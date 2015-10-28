/**
 * File: sequencer.h
 * Author: Sanjay Kannan
 * ---------------------
 * Creates a FluidSynth sequencer that allows
 * for arbitrary scheduling of note layers.
 */

#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <fluidsynth.h>
#include <map> // layers

#include "synthesizer.h"
#include "layer.h"
#include "ofMain.h"

// our access to C++11 is not guaranteed
typedef map<int, Layer>::iterator itType;

// used as a graphics callback function type
typedef vector<Block*> (*NoteHandler)(void*, int, int, int, int, int);

// sequences MIDI
class Sequencer {
  public:
    Sequencer();
    ~Sequencer();

    // initialize sequencer to a preset tempo, synthesizer, and note handler
    bool init(Synthesizer* synth, int beatsPerMinute, NoteHandler call, void* data);

    // add a sequence to be played on a given 
    // channel. it will start at next beat tick
    void writeLayer(int channel, Layer layer);

    // toggles muting on a given channel layer
    void toggleLayerIfExists(int channel);

    // get the global beat count
    int getGlobalBeatCount();

  protected:
    // run sequencer loop
    void scheduleLayers();
    void scheduleTimer();

    // actually schedule a note at the given time specified by date
    void sendNoteOn(int channel, short key, short velocity, unsigned int date);
    void sendNoteOff(int channel, short key, unsigned int date);

    // called when the timer scheduled by scheduleTimer goes off
    static void callback(unsigned int time, fluid_event_t* event, fluid_sequencer_t* seq, void* data);

    NoteHandler handler;
    void* callData;

    map<int, Layer> channels;
    short mySeqID, synthSeqID;
    unsigned int now;

    int globalBeatCount;
    int beatsPerMeasure;
    int beatsPerMinute;
    int msPerBeat;

    fluid_sequencer_t* sequencer;
    Synthesizer* fluid;
    ofMutex seqLock;
};

// guard
#endif
