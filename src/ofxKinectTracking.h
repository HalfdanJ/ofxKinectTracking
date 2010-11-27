/*
 *  ofxKinectTracking.h
 *  By Mads Høbye, Jonas Jongejan, Ole Kristensen
 *
 *  Created by Jonas Jongejan on 24/11/10.
 *
 */
#pragma once

#include "ofxKinect.h"
#include "ofMain.h"


class ofxKinectTracking {
public:
	int highlightGroup;
	ofxKinect * kinect;
	
	
	ofVideoGrabber  videoGrabber;
	unsigned char		*pixels;				// temp buffer

	
	void init();
	void update();
	void drawDepth(int x, int y, int w, int h);
	
	void clear();
	void resetBufferData();
};