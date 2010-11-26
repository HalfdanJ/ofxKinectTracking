/*
 *  ofxKinectTracking.h
 *  By Mads Høbye, Jonas Jongejan, Ole Kristensen
 *
 *  Created by Jonas Jongejan on 24/11/10.
 *
 */


#include "ofxKinectTracking.h"
#include "MSAOpenCL.h"

#define NUM_ANTS 307200 //Comes from 640*480

typedef struct{
	float lastValue;
	float value;
	int wakeUp;
	int group;
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
	ofSetFrameRate(30);
	kinect = new ofxKinect();
	kinect->init();
	kinect->setVerbose(true);
	kinect->open();
	ofSetVerticalSync(true);
	openCL.setupFromOpenGL();
	//openCL.setup(CL_DEVICE_TYPE_GPU, 2);
	/*ofTexture texture = ofTexture();
	 texture.allocate(640, 480, GL_LUMINANCE,false);
	 
	 cout<<"tex target: "<<texture.texData.textureTarget<<"  "<<GL_TEXTURE_2D<<
	 "  internalFormat: "<<texture.texData.glTypeInternal<<" "<<GL_RGB<<
	 "  glType:"<<texture.texData.glType<<" "<<GL_LUMINANCE<<
	 "  pixel format: "<<texture.texData.pixelType<<"  "<<GL_UNSIGNED_BYTE<<endl;
	 */
	//	clImage[0].initFromTexture(texture, CL_MEM_READ_ONLY, 0);	
	clImage[0].initFromTexture(kinect->getDepthTextureReference(), CL_MEM_READ_ONLY, 0);	
	clImage[1].initWithTexture(640, 480, GL_RGBA);
	
	int i=0;
	for(int y=0;y<480;y++){
		for(int x=0;x<640;x++){
			Ant &ant = ants[x + y*640];
			ant.lastValue = 0;
			ant.value = 0;
			ant.group = -1;
			ant.wakeUp = false;
			i++;
		}
	}
	
	clAntsBuffer.initBuffer(sizeof(Ant)*NUM_ANTS, CL_MEM_READ_WRITE, ants, true);
	
	sharedVariables[0] = 0;	
	clSharedBuffer.initBuffer(sizeof(int)*1, CL_MEM_READ_WRITE, sharedVariables, true);
	
	openCL.loadProgramFromFile("../../../../../addons/ofxKinectTracking/src/KinectTracking.cl");
	
	openCL.loadKernel("killing");
	openCL.loadKernel("spawning");
	openCL.loadKernel("updateImage");
	
	
	
	
	/*GLuint tex;
	 glGenTextures(1, &tex);
	 glBindTexture(GL_TEXTURE_2D, tex);
	 
	 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	 glTexImage2D(GL_TEXTURE_2D, 
	 0, 
	 GL_RGBA, 
	 (GLsizei)640,
	 (GLsizei)480, 
	 0, 
	 GL_LUMINANCE, 
	 GL_UNSIGNED_BYTE,
	 0);
	 glBindTexture(GL_TEXTURE_2D, 0);
	 
	 */
	
	
	
	/*
	 clImage[0].init(640,480, 1);
	 
	 cl_int err;
	 if(clImage[0].clMemObject) clReleaseMemObject(clImage[0].clMemObject);
	 
	 
	 clImage[0].clMemObject = clCreateFromGLTexture2D(clImage[0].pOpenCL->getContext(), CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, tex, &err);
	 assert(err != CL_INVALID_CONTEXT);
	 assert(err != CL_INVALID_VALUE);
	 //	assert(err != CL_INVALID_MIPLEVEL);
	 assert(err != CL_INVALID_GL_OBJECT);
	 assert(err != CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
	 assert(err != CL_OUT_OF_HOST_MEMORY);
	 assert(err == CL_SUCCESS);
	 assert(clImage[0].clMemObject);
	 */
	//texture = &tex;	
	
}

void ofxKinectTracking::update(){
	kinect->update();
	
	
	MSA::OpenCLKernel *kernel = openCL.kernel("killing");
	MSA::OpenCLKernel *kernel2 = openCL.kernel("spawning");
	MSA::OpenCLKernel *kernel3 = openCL.kernel("updateImage");
	
	//for(int i=0; i<3; i++) {
	cl_int2 spawnCoord = {int(ofRandom(0, 640)), int(ofRandom(0, 480))};
	if(ofRandom(0, 1) < 0.500){
		spawnCoord[0] = -1;
		spawnCoord[0] = -1;			
	}
	
	double totalTimeTmp;
	uint64_t mbeg, mend;
	openCL.finish();
	mbeg = mach_absolute_time();
	
	kernel->setArg(0, clImage[0].getCLMem());
/*	kernel->setArg(1, clImage[1].getCLMem());*/
	kernel->setArg(1, clAntsBuffer.getCLMem());
/*	kernel->setArg(3, clSharedBuffer.getCLMem());
/*	kernel->setArg(4, spawnCoord);*/
	kernel->run2D(640, 480);
	openCL.finish();
	
	mend = mach_absolute_time();
	killingTime.push_back(machcore(mend, mbeg));
	totalTimeTmp = machcore(mend, mbeg);
	mbeg = mach_absolute_time();
	
	int local_work_size = 64;	
	size_t shared_size = (1 * local_work_size) * sizeof(int);
	
	//kernel2->setArg(0, clImage[0].getCLMem());
	//kernel2->setArg(1, clImage[1].getCLMem());
	kernel2->setArg(0, clAntsBuffer.getCLMem());
	kernel2->setArg(1, clSharedBuffer.getCLMem());
	kernel2->setArg(2, spawnCoord);
	clSetKernelArg(kernel2->getCLKernel(), 3, shared_size, NULL);
	for(int i=0;i<5;i++){
		kernel2->run2D(640, 480,8,8);
	}
	openCL.finish();
	
	mend = mach_absolute_time();
	spawningTime.push_back(machcore(mend, mbeg));
	totalTimeTmp += machcore(mend, mbeg);
	mbeg = mach_absolute_time();
	
	kernel3->setArg(0, clImage[0].getCLMem());
	kernel3->setArg(1, clImage[1].getCLMem());
	kernel3->setArg(2, clAntsBuffer.getCLMem());
	kernel3->setArg(3, clSharedBuffer.getCLMem());
	kernel3->setArg(4, spawnCoord);
	kernel3->run2D(640, 480);
	/**/
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
	
	//}
	
}

void ofxKinectTracking::drawDepth(int x, int y, int w, int h){
	ofSetColor(255, 255, 255);
	openCL.finish();
	
	kinect->drawDepth(x,y,w,h);
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
			//		cout<<totalTime[i]*10000.0<<endl;
			glVertex2d(i*3, -totalTime[i]*scale);
			avg += totalTime[i];
		}
		glEnd();
		avg /=totalTime.size()-10;
		
		ofSetColor(255, 100, 255,255);
		ofLine(0, -avg*scale, 640*2, -avg*scale);
		ofDrawBitmapString("Avg: "+ofToString(avg*10000, 2), ofPoint(5,-avg*scale-10));
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
