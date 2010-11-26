/*
 *  ofxKinectTracking.h
 *  By Mads Høbye, Jonas Jongejan, Ole Kristensen
 *
 *  Created by Jonas Jongejan on 24/11/10.
 *
 */
#pragma once

#include "ofxKinect.h"
class ofxKinectTracking {
public:
	int highlightGroup;
	ofxKinect * kinect;
	
	
	void init();
	void update();
	void drawDepth(int x, int y, int w, int h);
};