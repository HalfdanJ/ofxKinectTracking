/*
 *  ofxKinectTracking.h
 *  By Mads Høbye, Jonas Jongejan, Ole Kristensen
 *
 *  Created by Jonas Jongejan on 24/11/10.
 *
 */

//#define NORMAL_CAMERA
#define NUM_ITERATIONS 5


#include "ofxKinectTracking.h"
#include "MSAOpenCL.h"

#define NUM_ANTS 307200 //Comes from 640*480

typedef struct{
	float lastValue;
	float value;
	int group;
	unsigned int groupTime;
	int lastGroup;
	unsigned int lastGroupTime;
	float lastGroupValue;
} Ant;


MSA::OpenCL			openCL;
MSA::OpenCLImage	clImage[2];				// two OpenCL images	

Ant ants[NUM_ANTS];
MSA::OpenCLBuffer clAntsBuffer;

int sharedVariables[1];
MSA::OpenCLBuffer clSharedBuffer;



#include <mach/mach_time.h>

vector<double> totalTime;
vector<double> killingTime;
vector<double> spawningTime;
vector<double> updateTime;

double machcore(uint64_t endTime, uint64_t startTime){	
	uint64_t difference = endTime - startTime;
    static double conversion = 0.0;
	double value = 0.0;
	
    if( 0.0 == conversion )
    {
        mach_timebase_info_data_t info;
        kern_return_t err = mach_timebase_info( &info );
        
        if( 0 == err ){
			/* seconds */
            conversion = 1e-9 * (double) info.numer / (double) info.denom;
			/* nanoseconds */
			//conversion = (double) info.numer / (double) info.denom;
		}
    }
    
	value = conversion * (double) difference;
	
	return value;
}


void ofxKinectTracking::init(){
	openCL.setupFromOpenGL();
	
#ifndef NORMAL_CAMERA
	kinect = new ofxKinect();
	kinect->init();
	kinect->setVerbose(true);
	kinect->open();
	
	
	
	//The depth image
	clImage[0].initFromTexture(kinect->getDepthTextureReference(), CL_MEM_READ_ONLY, 0);	
#else
	videoGrabber.initGrabber(640, 480, true);
	clImage[0].initWithTexture(640, 480, GL_LUMINANCE, CL_MEM_READ_ONLY);
	pixels		= new unsigned char[640*480];
	
#endif
	
	//The debug image
	clImage[1].initWithTexture(640, 480, GL_RGBA);
	
	//Intialize the ants buffer
	resetBufferData();
	clAntsBuffer.initBuffer(sizeof(Ant)*NUM_ANTS, CL_MEM_READ_WRITE, ants, true);
	
	//Buffer of shared variables
	sharedVariables[0] = 0;	
	clSharedBuffer.initBuffer(sizeof(int)*1, CL_MEM_READ_WRITE, sharedVariables, true);
	
	openCL.loadProgramFromFile("../../../../../addons/ofxKinectTracking/src/KinectTracking.cl");
	
	openCL.loadKernel("preUpdate");
	openCL.loadKernel("update");
	openCL.loadKernel("postUpdate");
}

void ofxKinectTracking::update(){
	bool newFrame;
	
#ifndef NORMAL_CAMERA
	kinect->update();	
	newFrame = kinect->isFrameNew();
	
#else
	videoGrabber.update();
	newFrame = videoGrabber.isFrameNew();
	if(newFrame){
		
		
		int pixelIndex = 0;
		for(int i=0; i<640; i++) {
			for(int j=0; j<480; j++) {
				int indexRGB	= pixelIndex * 3;
				int indexL	= pixelIndex;
				
				pixels[indexL] = videoGrabber.getPixels()[indexRGB+1  ];
				pixelIndex++;
			}
		}
		
		
		// write the new pixel data into the OpenCL Image (and thus the OpenGL texture)
		clImage[0].write(pixels);
	}
	
#endif
	if(newFrame){
		cl_int2 spawnCoord = {int(ofRandom(0, 640)), int(ofRandom(0, 480))};
		if(ofRandom(0, 1) < 0.500){
			spawnCoord[0] = -1;
			spawnCoord[0] = -1;			
		}
		
		
		MSA::OpenCLKernel *preKernel = openCL.kernel("preUpdate");
		MSA::OpenCLKernel *updateKernel = openCL.kernel("update");
		MSA::OpenCLKernel *postKernel = openCL.kernel("postUpdate");
		
		
		//Prepare timetaking 
		double totalTimeTmp;
		uint64_t mbeg, mend;
		openCL.finish();
		mbeg = mach_absolute_time();
		
		//Run pre update kernel
		preKernel->setArg(0, clImage[0].getCLMem());
		preKernel->setArg(1, clAntsBuffer.getCLMem());	
		preKernel->run2D(640, 480);
		
		//Calculate time
		openCL.finish();		
		mend = mach_absolute_time();
		killingTime.push_back(machcore(mend, mbeg));
		totalTimeTmp = machcore(mend, mbeg);
		mbeg = mach_absolute_time();
		
		//Run update kernel
		size_t shared_size = (1 * 64) * sizeof(int);		
		updateKernel->setArg(0, clAntsBuffer.getCLMem());
		updateKernel->setArg(1, clSharedBuffer.getCLMem());
		updateKernel->setArg(2, spawnCoord);
		clSetKernelArg(updateKernel->getCLKernel(), 3, shared_size, NULL);
		for(int i=0;i<NUM_ITERATIONS;i++){
			updateKernel->run2D(640, 480);
		}
		
		//Calculate time
		openCL.finish();		
		mend = mach_absolute_time();
		spawningTime.push_back(machcore(mend, mbeg));
		totalTimeTmp += machcore(mend, mbeg);
		mbeg = mach_absolute_time();
		
		//Run post update kernel
		postKernel->setArg(0, clImage[0].getCLMem());
		postKernel->setArg(1, clImage[1].getCLMem());
		postKernel->setArg(2, clAntsBuffer.getCLMem());
		postKernel->setArg(3, clSharedBuffer.getCLMem());
		postKernel->setArg(4, spawnCoord);
		postKernel->run2D(640, 480);
		
		//Calculate time
		openCL.finish();		
		mend = mach_absolute_time();
		updateTime.push_back(machcore(mend, mbeg));
		totalTimeTmp += machcore(mend, mbeg);		
		totalTime.push_back(totalTimeTmp);
		if(totalTime.size() > (640*2)/3){
			totalTime.erase(totalTime.begin());
			killingTime.erase(killingTime.begin());
			spawningTime.erase(spawningTime.begin());
			updateTime.erase(updateTime.begin());
		}
		
	}
}

void ofxKinectTracking::clear(){
	resetBufferData();
//	clAntsBuffer.initBuffer(sizeof(Ant)*NUM_ANTS, CL_MEM_READ_WRITE, ants, true);
	clAntsBuffer.write(ants, 0, sizeof(Ant)*NUM_ANTS, true);
}
void ofxKinectTracking::resetBufferData(){
	//Intialize the ants buffer
	for(int y=0;y<480;y++){
		for(int x=0;x<640;x++){
			Ant &ant = ants[x + y*640];
			ant.lastValue = 0;
			ant.value = 0;
			ant.group = -1;
			ant.groupTime = 0;
			ant.lastGroup = -1;
			ant.lastGroupTime = 0;
			ant.lastGroupValue = 0;
		}
	}	
}

void ofxKinectTracking::drawDepth(int x, int y, int w, int h){
	ofSetColor(255, 255, 255);
	openCL.finish();
#ifndef NORMAL_CAMERA
	kinect->drawDepth(x,y,w,h);
#else
	clImage[0].draw(x,y,w,h);
#endif
	clImage[1].draw(x+w, y, w,h);
	
	ofSetColor(255, 0, 0);
	ofDrawBitmapString(ofToString(ofGetFrameRate(), 1), ofPoint(20,20));
	
	ofEnableAlphaBlending();
	glTranslated(0, 480*2, 0);
	
	float scale = 5000.0;
	
	{
		ofSetColor(255, 255, 255,100);
		double avg = 0;
		glBegin(GL_LINE_STRIP);
		for(int i=10;i<totalTime.size();i++){
			glVertex2d(i*3, -totalTime[i]*scale);
			avg += totalTime[i];
		}
		glEnd();
		avg /=totalTime.size()-10;
		
		ofSetColor(255, 100, 255,255);
		ofLine(0, -avg*scale, 640*2, -avg*scale);
		ofDrawBitmapString("Avg: "+ofToString(avg*10000, 2), ofPoint(5,-avg*scale-10));
		
		double percentage = ((avg)/(1/30.0))*100;
		ofSetColor(255, 255, 255);
		ofDrawBitmapString("Time percentage of one frame used on openCL: "+ofToString(percentage, 2)+"%", ofPoint(5,-460));
	}
	{
		ofSetColor(255, 0, 0,150);
		double avg = 0;
		glBegin(GL_LINE_STRIP);
		for(int i=10;i<killingTime.size();i++){
			//		cout<<totalTime[i]*10000.0<<endl;
			glVertex2d(i*3, -killingTime[i]*scale);
			avg += killingTime[i];
		}
		glEnd();
		avg /=killingTime.size()-10;
		
		ofSetColor(255, 100, 0);
		ofLine(0, -avg*scale, 640*2, -avg*scale);
		ofDrawBitmapString("K Avg: "+ofToString(avg*10000, 2), ofPoint(5,-avg*scale-10));
	}
	
	{
		ofSetColor(0, 255, 0,150);
		double avg = 0;
		glBegin(GL_LINE_STRIP);
		for(int i=10;i<spawningTime.size();i++){
			//		cout<<totalTime[i]*10000.0<<endl;
			glVertex2d(i*3, -spawningTime[i]*scale);
			avg += spawningTime[i];
		}
		glEnd();
		avg /=spawningTime.size()-10;
		
		ofSetColor(100, 255, 0);
		ofLine(0, -avg*scale, 640*2, -avg*scale);
		ofDrawBitmapString("S Avg: "+ofToString(avg*10000, 2), ofPoint(5,-avg*scale-10));
	}
	{
		ofSetColor(0, 0, 255,150);
		double avg = 0;
		glBegin(GL_LINE_STRIP);
		for(int i=10;i<updateTime.size();i++){
			//		cout<<totalTime[i]*10000.0<<endl;
			glVertex2d(i*3, -updateTime[i]*scale);
			avg += updateTime[i];
		}
		glEnd();
		avg /=updateTime.size()-10;
		
		ofSetColor(0, 100, 255);
		ofLine(0, -avg*scale, 640*2, -avg*scale);
		ofDrawBitmapString("U Avg: "+ofToString(avg*10000, 2), ofPoint(5,-avg*scale-10));
	}
	
	
	
	
	
}
