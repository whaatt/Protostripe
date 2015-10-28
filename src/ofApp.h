/**
 * File: ofApp.h
 * Author: Sanjay Kannan
 * ---------------------
 * Header file for the entire
 * OpenFrameworks application.
 */

#pragma once
#include <vector>

#include "synthesizer.h"
#include "sequencer.h"
#include "mapper.h"
#include "ofMain.h"

// master OpenFrameworks runner
class ofApp : public ofBaseApp {
  public:
    void setup();
    void update();
    void draw();

    // some usual boilerplate
    void keyPressed(int key);
    void keyReleased(int key);
    void windowResized(int width, int height);

    // graphics callback from sequencer [to create blocks]
    static vector<Block*> noteHandler(void* instance, int channel,
      int position, int velocity, int distance, int duration);

  private:
    // originally by Ge Wang
    Synthesizer* synth = NULL;
    Sequencer* seq = NULL;
    int beatsPerMinute = 120;
    int beatsPerMeasure = 4;

    // audio state variables
    Mapper mapper; // note maps
    bool freePlayMuted = false;
    bool recordingMode = false;
    int recordingChannel = 1;
    int recordingBeat = 0;
    long recordingTime = 0;

    // store to build layers
    map<char, long> keyTimes;
    map<char, int> keyPitches;
    map<char, int> keyPositions;
    map<char, int> keyVelocities;
    vector<Note> recordedNotes;

    // used to create and finalize blocks
    map<char, vector<Block*> > keyBlocks;

    // represent Mondrian as
    // a collection of stripes
    vector<LayerStripe> stripes;
    void makeGridStripes();
    int stripeCount = 16;
    int screenSize = 2;
    ofMutex stripeLock;

    // for listing in the UI
    map<string, int> instMap;
    vector<string> instruments;
    vector<string> scales;
    vector<string> modes;
    vector<string> keys;

    // for correctly rendering text within stripes
    void textOnHorizontal(int index, float posFrac, string text, ofColor color);
    void numOnVertical(int index, float posFrac, string text, ofColor color);
    ofTrueTypeFont myFont;
    bool displayText = true;
    float scaleFactor;

    // mapping state
    int instIndex;
    int scaleIndex;
    int modeIndex;
    int keyIndex;

    // track volume control
    int currentVelocity = 127;

    // build with metronome
    void buildSequencer();
    void destroySequencer();
};
