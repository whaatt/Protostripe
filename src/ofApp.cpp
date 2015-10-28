/**
 * File: ofApp.cpp
 * Author: Sanjay Kannan
 * ---------------------
 * TODO: Write description.
 */

#include <set>
#include <vector>
#include <algorithm>
#include <sstream>

#include <math.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fluidsynth.h>

#include "sequencer.h"
#include "mapper.h"
#include "layer.h"
#include "ofApp.h"

// sampled from a Mondrian
// ofColor RED(228, 49, 55);
// ofColor BLUE(60, 71, 153);
// ofColor YELLOW(243, 197, 39);

// Broadway Boogie Woogie
ofColor WHITE(255, 255, 255);
ofColor GRAY(145, 145, 145, 190);
ofColor SHALE(95, 95, 95);
ofColor RED(168, 16, 15, 190);
ofColor BLUE(27, 61, 147, 190);
ofColor YELLOW(233, 199, 34);
ofColor FADED(233, 199, 34, 95);
ofColor CHARTREUSE(151, 209, 30);
ofColor BLACK(0, 0, 0);

// number shift keys
string SHIFTS("#$%^&*");

/**
 * Function: now
 * -------------
 * Returns the current UNIX time
 * in milliseconds from the epoch.
 */
long long now() {
  struct timeval tp; // from time.h
  gettimeofday(&tp, NULL); // specified in POSIX
  return (long long) tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

/**
 * Function: readInstruments
 * -------------------------
 * Read in a file mapping from instrument
 * names to their General MIDI equivalents.
 */
void readInstruments(const string instFileName,
  map<string, int>& instMap, vector<string>& instruments) {
  ifstream instFile(instFileName.c_str());
  string instName; int instMIDI;

  // easiest way to read key value file in CPP
  while (instFile >> instName >> instMIDI) {
    // treat an Electric_Guitar as an Electric Guitar
    replace(instName.begin(), instName.end(), '_', ' ');
    instMap[instName] = instMIDI;
    instruments.push_back(instName);
  }
}

/**
 * Function: selectPair
 * --------------------
 * Selects two random indices on the range
 * [0, n] uniformly. Taken from StackOverflow.
 */
void selectPair(int size, int& first, int& second) {
  // pick a random element
  first = rand () * size / RAND_MAX;
  // pick a random element from remaining
  second = rand () * (size - 1) / RAND_MAX;
  // adjust second because of first
  if (second >= first) ++second;
}

/**
 * Function: setup
 * ---------------
 * Initializes synth.
 */
void ofApp::setup() {
  // initialization code here borrowed
  // and modified from the OpenFrameworks
  // audio input example

  // ofEnableSmoothing();
  ofSetVerticalSync(true);
  ofSetCircleResolution(80);
  ofBackground(WHITE);

  // 256 is polyphony
  synth = new Synthesizer();
  synth -> init(44100, 256, true);
  synth -> load("data/fluid.sf2");

  // we could add error checking here in the future
  readInstruments("data/instruments.txt", instMap, instruments);
  mapper.init("data/scales.txt", "data/modes.txt");

  // get UI listing variables
  scales = mapper.getScales();
  modes = mapper.getModes();
  keys = mapper.getKeys();

  instIndex = 0;
  scaleIndex = 0;
  modeIndex = 0;
  keyIndex = 0;

  // set free play channel to starting default instrument
  synth -> setInstrument(1, instMap[instruments[instIndex]]);

  // initialize font to Roboto
  myFont.loadFont("font.ttf", 10);
  myFont.setSpaceSize(0.55);

  // initialize Manhattan
  makeGridStripes();
}

/**
 * Function: noteHandler
 * ---------------------
 * Handles note notifications graphically and
 * returns a vector of new block references.
 */
vector<Block*> ofApp::noteHandler(void* instance, int channel,
  int position, int velocity, int distance, int duration) {
  ofApp* app = (ofApp*) instance; // passed as this
  int msPerBeat = 60000 / app -> beatsPerMinute;

  // reindex channel from zero
  channel = channel - 1;

  float msScreen = app -> screenSize * msPerBeat * app -> beatsPerMeasure;
  float posFrac = (float) distance / msScreen;
  float sizeFrac = (float) duration / msScreen;
  float stripeSizeF = 1.0 / 60.0;

  float posFracDirectedA; // posFrac changes by channel direction
  if (app -> stripes[channel].forward) posFracDirectedA = -sizeFrac - posFrac;
  else posFracDirectedA = 1.0 + posFrac; // easy enough backwards

  float posFracDirectedB; // posFrac changes by channel direction
  if (app -> stripes[channel + 8].forward) posFracDirectedB = -sizeFrac - posFrac;
  else posFracDirectedB = 1.0 + posFrac; // easy enough backwards

  ofColor colors[] = {BLUE, RED, BLUE, RED, GRAY};
  ofColor color = colors[position % 5]; // we do not like gray
  bool finalized = channel != 0; // free play expands blocks
  float velFrac = (float) velocity / 127.0;
  app -> stripeLock.lock();

  Block* blockA = new Block;
  blockA -> posFrac = posFracDirectedA;
  blockA -> sizeFrac = sizeFrac;
  blockA -> finalized = finalized;
  blockA -> velFrac = velFrac;
  blockA -> color = color;

  Block* blockB = new Block;
  blockB -> posFrac = posFracDirectedB;
  blockB -> sizeFrac = sizeFrac;
  blockB -> finalized = finalized;
  blockB -> velFrac = velFrac;
  blockB -> color = color;

  app -> stripes[channel].blocks.push_back(blockA);
  app -> stripes[channel + 8].blocks.push_back(blockB);

  vector<Block*> newBlocks;
  newBlocks.push_back(blockA);
  newBlocks.push_back(blockB);

  // used for finalizing
  app -> stripeLock.unlock();
  return newBlocks;
}

/**
 * Function: makeGridStripes
 * -------------------------
 * Initializes a count of stripes
 * and adds it to the stripe buffer.
 */
void ofApp::makeGridStripes() {
  // stripeSizeF is of smaller dimension
  int now = ofGetElapsedTimeMillis();
  float stripeSizeF = 1.0 / 40.0;
  vector<int> vStripeIndices;
  vector<int> hStripeIndices;

  // establish valid x positions for vertical stripes
  for (int i = 1; i < 1.0 / stripeSizeF - 1; i += 2)
    vStripeIndices.push_back(i);

  // establish valid y positions for horizontal stripes
  for (int i = 1; i < 1.0 / stripeSizeF - 1; i += 2)
    hStripeIndices.push_back(i);

  // uniformly distribute stripes across the screen
  random_shuffle(vStripeIndices.begin(), vStripeIndices.end());
  random_shuffle(hStripeIndices.begin(), hStripeIndices.end());

  // avoid races
  stripeLock.lock();

  // generate stripes for all layers plus extra
  for (int i = 0; i < stripeCount; i += 1) {
    // draw new vertical stripe
    if (i < stripeCount / 2) {
      if (vStripeIndices.size() == 0) continue;

      float posFrac = vStripeIndices.back() * stripeSizeF;
      float sizeFrac = stripeSizeF; // TODO: varying widths?
      bool visible = true; //i < 1 || i > 8;

      vStripeIndices.pop_back(); // mark the index as used
      stripes.push_back({vector<Block*>(), now, false, rand() % 2, posFrac, sizeFrac, visible});
    }

    else { // draw new horizontal stripe
      if (hStripeIndices.size() == 0) continue;

      float posFrac = hStripeIndices.back() * stripeSizeF;
      float sizeFrac = stripeSizeF; // TODO: varying widths?
      bool visible = true; //i < 1 || i > 8;

      hStripeIndices.pop_back(); // mark the index as used
      stripes.push_back({vector<Block*>(), now, true, rand() % 2, posFrac, sizeFrac, visible});
    }
  }

  // all done
  stripeLock.unlock();
}

/**
 * Function: update
 * ----------------
 * Moves stripes and generates them if we
 * need more grid stripes or color blocks.
 */
void ofApp::update() {
  // get current time in seconds
  int now = ofGetElapsedTimeMillis();
  int msPerBeat = 60000 / beatsPerMinute;
  int speed = screenSize * msPerBeat * beatsPerMeasure;

  // avoid races
  stripeLock.lock();

  // loop through blocks in layers to update pos
  for (int i = 0; i < stripes.size(); i += 1) {
    bool forward = stripes[i].forward; // direction
    int diff = now - stripes[i].lastTime; // time difference

    for (int j = stripes[i].blocks.size() - 1; j >= 0; j -= 1) {
      Block* block = stripes[i].blocks[j];

      // try to delete block in forward direction
      if (forward && block -> posFrac > 1.5) {
        delete stripes[i].blocks[j]; // heap allocated
        stripes[i].blocks.erase(stripes[i].blocks.begin() + j);
        continue; // block went off screen and is deleted
      }

      // try to delete block in backward direction
      else if (!forward && block -> posFrac + block -> sizeFrac < -0.5) {
        delete stripes[i].blocks[j]; // heap allocated
        stripes[i].blocks.erase(stripes[i].blocks.begin() + j);
        continue; // block went off screen and is deleted
      }

      // for free play, block size becomes
      // larger on a prolonged press
      if (!block -> finalized) {
        if (forward) // grow before releasing down or right
          block -> sizeFrac += (float) diff / (float) speed;

        else { // release and grow up or left
          block -> sizeFrac += (float) diff / (float) speed;
          block -> posFrac -= (float) diff / (float) speed;
        }
      }

      else { // TODO: should we have per block lastTime values?
        if (forward) block -> posFrac += (float) diff / (float) speed;
        else block -> posFrac -= (float) diff / (float) speed;
      }
    }

    // so diff stays relative to last render
    stripes[i].lastTime = now;
  }

  // all done
  stripeLock.unlock();
}

/**
 * Function: textOnHorizontal
 * --------------------------
 * Correctly renders text on
 * a horizontal stripe by index.
 */
void ofApp::textOnHorizontal(int index, float posFrac, string text, ofColor color) {
  if (!displayText) return; // no text distraction free mode
  ofSetColor(color); // maybe make this a parameter DONE
  float stripeSizeF = 1.0 / 40.0;

  // calculate rendering on stripe
  int xPos = posFrac * ofGetWidth();
  int yPos = stripes[index].posFrac * ofGetHeight();
  int yTrans = stripeSizeF * 0.7 * min(ofGetHeight(), ofGetWidth());
  myFont.drawString(text, xPos, yPos + yTrans);
}

/**
 * Function: textOnVertical
 * --------------------------
 * Correctly renders a single number
 * somewhere along a vertical stripe.
 */
void ofApp::numOnVertical(int index, float posFrac, string text, ofColor color) {
  if (!displayText) return; // no text distraction free mode
  ofSetColor(color); // maybe make this a parameter DONE
  float stripeSizeF = 1.0 / 40.0;

  // calculate rendering on stripe
  int xPos = stripes[index].posFrac * ofGetWidth();
  int yPos = posFrac * ofGetHeight();
  int xTrans = stripeSizeF * 0.3 * min(ofGetHeight(), ofGetWidth());
  myFont.drawString(text, xPos + xTrans, yPos);
}

/**
 * Function: draw
 * --------------
 * Draws stripe buffers.
 */
void ofApp::draw() {
  // portmanteau of prototype and stripe
  ofSetWindowTitle("Protostripe");

  // get smaller dimension for drawing stripe widths
  int smallDim = min(ofGetWidth(), ofGetHeight());

  // avoid races
  stripeLock.lock();

  // draw all of the grid stripes [layers]
  for (int i = 0; i < stripes.size(); i += 1) {
    if (!stripes[i].visible) continue;

    if (stripes[i].horizontal) { // horizontal stripe
      int width = ofGetWidth(); // full width
      int height = stripes[i].sizeFrac * smallDim;
      int x = 0; // start stripe at left
      int y = stripes[i].posFrac * ofGetHeight();

      // sequencer off or not the free play channel
      if (seq == NULL && i % 8) ofSetColor(FADED);
      else if (recordingMode && i % 8) ofSetColor(FADED);

      // recording and free play
      else if (recordingMode) {
        int correction = 300; // user error
        int diff = now() - recordingTime + correction;
        int msPerBeat = 60000 / beatsPerMinute;

        // count the user down visually
        if (diff / msPerBeat < beatsPerMeasure) {
          if (diff % msPerBeat < msPerBeat / 4)
            ofSetColor(CHARTREUSE);
          else ofSetColor(YELLOW);
        }

        // now capturing notes!
        else ofSetColor(CHARTREUSE);
      }

      // not recording and enabled
      else ofSetColor(YELLOW);
      ofRect(x, y, width, height);

      stringstream index; index << i - 8 + 1;
      float posFrac = stripes[i].forward ? 0.005 : 0.99;
      textOnHorizontal(i, posFrac, index.str(), SHALE);
    }

    else { // draw a vertical stripe
      int width = stripes[i].sizeFrac * smallDim;
      int height = ofGetHeight(); // full height
      int x = stripes[i].posFrac * ofGetWidth();
      int y = 0; // start stripe at top

      // sequencer off or not the free play channel
      if (seq == NULL && i % 8) ofSetColor(FADED);
      else if (recordingMode && i % 8) ofSetColor(FADED);

      // recording and free play
      else if (recordingMode) {
        int correction = 300; // user error
        int diff = now() - recordingTime + correction;
        int msPerBeat = 60000 / beatsPerMinute;

        // count the user down visually
        if (diff / msPerBeat < beatsPerMeasure) {
          if (diff % msPerBeat < msPerBeat / 4)
            ofSetColor(CHARTREUSE);
          else ofSetColor(YELLOW);
        }

        // now capturing notes!
        else ofSetColor(CHARTREUSE);
      }

      // not recording and enabled
      else ofSetColor(YELLOW);
      ofRect(x, y, width, height);

      stringstream index; index << i + 1;
      float posFrac = stripes[i].forward ? 0.02 : 0.995;
      numOnVertical(i, posFrac, index.str(), SHALE);
    }
  }

  stringstream BPM;
  stringstream BPMr;
  stringstream vol;
  BPM << beatsPerMinute << " BPM";
  BPMr << beatsPerMeasure << " Beats";
  vol << (int) round(100 * (float) currentVelocity / 127.0);

  textOnHorizontal(8, 0.85, BPM.str(), BLACK); // conversion above
  textOnHorizontal(9, 0.75, BPMr.str(), BLACK); // conversion above
  textOnHorizontal(10, 0.25, "Volume: " + vol.str() + "%", BLACK);
  textOnHorizontal(11, 0.45, "Scale: " + scales[scaleIndex], BLACK);
  textOnHorizontal(12, 0.55, "Instrument: " + instruments[instIndex], BLACK);
  textOnHorizontal(13, 0.35, "Key: " + keys[keyIndex], BLACK);
  textOnHorizontal(14, 0.15, "Protostripe 0.0.2", BLACK);
  textOnHorizontal(15, 0.65, "By Sanjay Kannan", BLACK);

  // draw all the blocks after to appear above
  for (int i = 0; i < stripes.size(); i += 1) {
    if (stripes[i].horizontal) { // on a horizontal stripe
      for (int j = 0; j < stripes[i].blocks.size(); j += 1) {
        int width = stripes[i].blocks[j] -> sizeFrac * ofGetWidth();
        int height = stripes[i].sizeFrac * smallDim;
        int x = stripes[i].blocks[j] -> posFrac * ofGetWidth();
        int y = stripes[i].posFrac * ofGetHeight();

        // add transparency if recording or volume low
        ofColor renderColor = stripes[i].blocks[j] -> color;
        if (recordingMode && i % 8) renderColor.a *= 1.5;
        else renderColor.a += 30.0 * stripes[i].blocks[j] -> velFrac;

        ofSetColor(renderColor);
        ofRect(x, y, width, height);
      }
    }

    else { // draw a block on a vertical stripe
      for (int j = 0; j < stripes[i].blocks.size(); j += 1) {
        int width = stripes[i].sizeFrac * smallDim;
        int height = stripes[i].blocks[j] -> sizeFrac * ofGetHeight();
        int x = stripes[i].posFrac * ofGetWidth();
        int y = stripes[i].blocks[j] -> posFrac * ofGetHeight();

        // add transparency if recording or volume low
        ofColor renderColor = stripes[i].blocks[j] -> color;
        if (recordingMode && i % 8) renderColor.a *= 1.5;
        else renderColor.a += 30.0 * stripes[i].blocks[j] -> velFrac;

        ofSetColor(renderColor);
        ofRect(x, y, width, height);
      }
    }
  }

  // all done
  stripeLock.unlock();
}

/**
 * Function: buildSequencer
 * ------------------------
 * Builds a sequencer and initializes
 * it with a metronome tick layer.
 */
void ofApp::buildSequencer() {
  seq = new Sequencer();
  seq -> init(synth, beatsPerMinute, &ofApp::noteHandler, this);

  int msPerBeat = 60000 / beatsPerMinute;
  int duration = msPerBeat / 2;

  Layer metronome;
  metronome.channel = 2; // metronome channel
  metronome.beatCount = beatsPerMeasure;

  metronome.notes.push_back({70, 127, 0 * msPerBeat, duration, 0});
  for (int i = 1; i < beatsPerMeasure; i += 1) // subsequent weak beats
    metronome.notes.push_back({60, 127, i * msPerBeat, duration, 0});

  // play metronome by default
  synth -> setInstrument(2, 115);
  seq -> writeLayer(2, metronome);
}

/**
 * Function: destroySequencer
 * --------------------------
 * Destroys the currently active
 * sequencer on the synth, including
 * all notes and layers written to it.
 */
void ofApp::destroySequencer() {
  delete seq;
  seq = NULL;
}

/**
 * Function: keyPressed
 * --------------------
 * Handles key presses.
 */
void ofApp::keyPressed(int key) {
  // time signature control with ()
  if (key == '(' && beatsPerMeasure > 1)
    if (seq == NULL) beatsPerMeasure -= 1;
  if (key == ')' && beatsPerMeasure < 24)
    if (seq == NULL) beatsPerMeasure += 1;

  // tempo control with 90
  if (key == '9' && beatsPerMinute > 24)
    if (seq == NULL) beatsPerMinute -= 1;
  if (key == '0' && beatsPerMinute < 200)
    if (seq == NULL) beatsPerMinute += 1;

  // note velocity control with -=
  if (key == '-' && currentVelocity > 0)
    currentVelocity -= 1;
  if (key == '=' && currentVelocity < 127)
    currentVelocity += 1;

  // scale setting control with []
  if (key == ';' && scaleIndex > 0)
    mapper.setScaleIndex(--scaleIndex);
  if (key == '\'' && scaleIndex < scales.size() - 1)
    mapper.setScaleIndex(++scaleIndex);

  // control free play sound with ;'
  if (key == '[' && instIndex > 0)
    synth -> setInstrument(1, instMap[instruments[--instIndex]]);
  if (key == ']' && instIndex < instruments.size() - 1)
    synth -> setInstrument(1, instMap[instruments[++instIndex]]);

  // key setting control with ,.
  if (key == ',' && keyIndex > 0)
    mapper.setKeyIndex(--keyIndex);
  if (key == '.' && keyIndex < keys.size() - 1)
    mapper.setKeyIndex(++keyIndex);

  // toggle chromatic keyboard mapping
  if (key == '/' && modeIndex == 1)
    mapper.setModeIndex(--modeIndex);
  else if (key == '/' && modeIndex == 0)
    mapper.setModeIndex(++modeIndex);

  // assorted graphical keys
  if (key == '\\') ofToggleFullscreen();
  if (key == '|') displayText = !displayText;

  // special muting for the free play layer
  if (key == '1') freePlayMuted = !freePlayMuted;

  // toggle muting on a layer
  if (key >= '2' && key <= '8') {
    if (seq == NULL) return;
    int keyI = key - '0'; // ASCII
    seq -> toggleLayerIfExists(keyI);
  }

  // allow velocity modifier on shift
  int noteVelocity = currentVelocity;
  if (key >= 'A' && key <= 'Z') {
    noteVelocity *= 2;
    noteVelocity /= 3;
    key += 32; // toLower
  }

  // start playing a given note
  if (key >= 'a' && key <= 'z') {
    // controlled separately
    if (recordingChannel == 1)
      if (freePlayMuted) return;

    // avoid multiple notes for single press
    if (keyPitches.count(key)) return;

    int pitch = mapper.getNote(key);
    int position = mapper.getPosition(key);
    synth -> noteOn(1, pitch, noteVelocity);
    keyPitches[key] = pitch; // save start pitch
    keyPositions[key] = position; // save key position
    keyVelocities[key] = noteVelocity; // save velocity
    keyTimes[key] = now(); // save start time

    // create unfinalized blocks with zero size
    keyBlocks[key] = noteHandler(this, 1, position, noteVelocity, 0, 0);
  }
}

/**
 * Function: keyReleased
 * ---------------------
 * Handles key releases.
 */
void ofApp::keyReleased(int key) {
  // seq toggle key
  if (key == '`') {
    if (seq == NULL) buildSequencer();
    else destroySequencer();
    recordingMode = false;
  }

  // space to stop recording mode
  if (key == 32 && recordingMode) {
    if (seq == NULL || recordingChannel == 1) return; // sequencer sanity checks
    cout << "Stopping recording on channel " << recordingChannel << "." << endl;

    int beatCount = seq -> getGlobalBeatCount();
    float msPerBeat = 60000 / beatsPerMinute;
    long startDiff = now() - recordingTime;
    int startBeatDiff = round(startDiff / msPerBeat);

    Layer recorded; recorded.channel = recordingChannel;
    recorded.beatCount = startBeatDiff - beatsPerMeasure;
    recorded.beatStart = beatCount;

    // add all of the recorded notes to the new layer
    for (int i = 0; i < recordedNotes.size(); i += 1)
      recorded.notes.push_back(recordedNotes[i]);

    // start playing the layer immediately on channel
    int currentInst = instMap[instruments[instIndex]];
    synth -> setInstrument(recordingChannel, currentInst);
    seq -> writeLayer(recordingChannel, recorded);
    recordingMode = false;
    recordingChannel = 1;
  }

  // look for characters SHIFT two to eight
  unsigned int found = SHIFTS.find(key);
  if (found != string::npos) {
    int channel = found + 3; // starts at SHIFT + 3
    if (seq == NULL) return; // sequencer sanity check
    cout << "Recording notes on channel " << channel << "." << endl;

    recordingBeat = seq -> getGlobalBeatCount();
    recordingTime = now(); // UNIX ms
    recordingChannel = channel;
    recordedNotes.clear();
    recordingMode = true;
  }

  // allow velocity modifier shift
  if (key >= 'A' && key <= 'Z')
    key += 32; // toLower

  // stop playing a given note
  if (key >= 'a' && key <= 'z') {
    // controlled separately
    if (recordingChannel == 1)
      if (freePlayMuted) return;

    // make sure note has started playing
    if (!keyPitches.count(key)) return;

    // note that pitch may have
    // actually changed in the
    // meantime, but we use the
    // originally scheduled one
    int pitch = keyPitches[key];
    int position = keyPositions[key];
    int velocity = keyVelocities[key];
    long long currTime = now();

    // build up a note to add to recording layer
    if (recordingChannel != 1 && recordingMode) {
      float msPerBeat = 60000 / beatsPerMinute;

      // account for the fact that people
      // are not perfect in starting
      int correction = 300;

      int duration = currTime - keyTimes[key];
      long startDiff = keyTimes[key] - recordingTime;
      int offset = startDiff - msPerBeat * beatsPerMeasure + correction;

      // countdown done
      if (offset >= 0) {
        Note newNote = {pitch, velocity, offset, duration, position};
        recordedNotes.push_back(newNote);
      }
    }

    // turn the present note off
    synth -> noteOff(1, pitch);

    // finalize the note just played on screen
    for (int i = 0; i < keyBlocks[key].size(); i += 1)
      keyBlocks[key][i] -> finalized = true;

    // avoid weird overwriting complications
    keyVelocities.erase(keyVelocities.find(key));
    keyPositions.erase(keyPositions.find(key));
    keyBlocks.erase(keyBlocks.find(key));
    keyPitches.erase(keyPitches.find(key));
    keyTimes.erase(keyTimes.find(key));
  }
}

/**
 * Function: windowResized
 * -----------------------
 * Handles window resizing.
 */
void ofApp::windowResized(int width, int height) {
  float scaleFactor = (float) min(width, height) / 768.0;
  myFont.loadFont("font.ttf", 10 * scaleFactor);
  myFont.setSpaceSize(0.55);
}
