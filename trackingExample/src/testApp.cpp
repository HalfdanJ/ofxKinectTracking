#include "testApp.h"

#include "ofxKinectTracking.h"

//--------------------------------------------------------------
void testApp::setup(){
	ofSetWindowShape(640*2, 480*2);
	
	tracker = new ofxKinectTracking();
	tracker->init();

}

//--------------------------------------------------------------
void testApp::update(){
	tracker->update();
	
	
}

//--------------------------------------------------------------
void testApp::draw(){
	ofBackground(0, 0, 0);
	ofSetColor(255, 255, 255);
	tracker->drawDepth(0,0,640,480);

}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
	
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){
	
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){
	
}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){
	
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
//	highlightGroup = ants.at(y).at(x)->group;
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
	
}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){
	
}

