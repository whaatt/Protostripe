/**
 * File: layer.h
 * Author: Sanjay Kannan
 * ---------------------
 * Specifies a voicing layer
 * to be played by a sequencer
 * and their necessary graphical
 * equivalents to be drawn.
 */

#ifndef LAYER_H
#define LAYER_H

// TODO: use map?
#include <vector>
#include "ofMain.h"

// TODO: we might want to
// add graphical parameters
// or pitch bend to this
struct Note {
  // the building blocks of layers
  float pitch; // MIDI pitch value
  short velocity; // note hardness
  int msOffset; // offset from start
  int msDuration; // note duration
  int position; // keyboard position
};

// because a class seems sort of
// unnecessary without methods
struct Layer {
  // -1 is a sentinel for paused layers
  Layer() : muted(false), beatStart(-1) {}

  vector<Note> notes; // vector of every layer note
  int beatStart; // beat count at which it was enabled
  int beatCount; // number of beats in layer sequence
  bool muted; // whether the layer is audible
  int channel; // what channel to associate with
};

// graphical note
struct Block {
  float posFrac;
  float sizeFrac;
  ofColor color;
  float velFrac;
  bool finalized;
};

// yellow street stripes
struct LayerStripe {
  // each of the color things
  // that appear on the stripes
  vector<Block*> blocks;
  int lastTime;

  bool horizontal;
  bool forward;
  float posFrac;
  float sizeFrac;
  bool visible;
};

// guard
#endif
