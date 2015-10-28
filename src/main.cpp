/**
 * File: main.cpp
 * Author: Sanjay Kannan
 * ---------------------
 * Initializes OpenFrameworks
 * and runs the windowed app.
 */

#include "ofMain.h"
#include "ofApp.h"
#include "ofAppGlutWindow.h"

/**
 * Function: main
 * --------------
 * Sets up OpenFrameworks
 * and runs the window thread.
 */
int main() {
  // set up the OpenGL context in window
  ofAppGlutWindow window; // mirroring
  ofSetupOpenGL(&window, 1024, 768, OF_WINDOW);

  cout << "Protostripe: Mondrian Makes A DAW" << endl;
  cout << "By Sanjay Kannan for MUSIC 256A" << endl;
  cout << "Last Updated: 27 October 2015" << endl << endl;

  // this kicks off the running of my app
  // can be OF_WINDOW or OF_FULLSCREEN
  // pass in width and height too:
  ofRunApp(new ofApp());
}
