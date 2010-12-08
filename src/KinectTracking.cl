
#define dieThreshold 2/255.0//0.03921568627
#define friendThreshold 1/255.0 //0.007843137255
#define switchBackThreshold 2/255.0

typedef struct{
	float lastValue;
	float value;
	int group;
	unsigned int groupTime;
	int lastGroup;
	unsigned int lastGroupTime;
	float lastGroupValue;
} Ant;




void HsvToRgb(float h, float S, float V, float * r, float * g, float * b)
{
	// ######################################################################
	// T. Nathan Mundhenk
	// mundhenk@usc.edu
	// C/C++ Macro HSV to RGB
	
	double H = h;
	while (H < 0) { H += 360; };
	while (H >= 360) { H -= 360; };
	double R, G, B;
	if (V <= 0)
    { R = G = B = 0; }
	else if (S <= 0)
	{
		R = G = B = V;
	}
	else
	{
		float hf = H / 60.0;
		int i = (int)floor(hf);
		float f = hf - i;
		float pv = V * (1 - S);
		float qv = V * (1 - S * f);
		float tv = V * (1 - S * (1 - f));
		switch (i)
		{			
			// Red is the dominant color			
			case 0:
			R = V;
			G = tv;
			B = pv;
			break;
			
			// Green is the dominant color
			
			case 1:
			R = qv;
			G = V;
			B = pv;
			break;
			case 2:
			R = pv;
			G = V;
			B = tv;
			break;
			
			// Blue is the dominant color
			
			case 3:
			R = pv;
			G = qv;
			B = V;
			break;
			case 4:
			R = tv;
			G = pv;
			B = V;
			break;
			
			// Red is the dominant color
			
			case 5:
			R = V;
			G = pv;
			B = qv;
			break;
			
			// Just in case we overshoot on our math by a little, we put these here. Since its a switch it won't slow us down at all to put these here.
			
			case 6:
			R = V;
			G = tv;
			B = pv;
			break;
			case -1:
			R = V;
			G = pv;
			B = qv;
			break;
			
			// The color is not defined, we should throw an error.
			
			default:
			//LFATAL("i Value error in Pixel conversion, Value is %d", i);
			R = G = B = V; // Just pretend its black/white
			break;
		}
	}
	
	*r = R;
	*g = G;
	*b = B;
}



//This kernel vil update the pixelvalue for the ant, and see if it should die because of a change
__kernel void preUpdate(read_only image2d_t srcImage,  __global Ant* pIn){        
	int2 coords = (int2)(get_global_id(0), get_global_id(1));	
	
	//Get the ant at this coordinate
	__global Ant *a = &pIn[coords.x + coords.y*640];
	
	//Sample the color in this pixel
	sampler_t smp = CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;	
	float value  = read_imagef(srcImage, smp, coords).w;
	a->value = value;
	
	//Kill ants that have moved to fast
	if(a->group != -1 && fabs(value - a->lastValue) > dieThreshold ){
		
		if(a->groupTime > a->lastGroupTime)
		{
			a->lastGroupTime = a->groupTime;
			a->lastGroupValue = a->lastValue;
			a->lastGroup = a->group;
		}
		
		a->group = -1;
		a->groupTime = 0;
		
	}
	
	
	
	//Update lastValue to the current value
	a->lastValue = value;
	if(	a->group != -1)
	{
		a->groupTime ++;
	}
	
}



//This kernel will spawn new ants, and after that try and seed new ants. 
__kernel void update(__global Ant* pIn, __global int * sharedVariables, const int2 spawnCoord, __local int * shared){        
	int2 coords = (int2)(get_global_id(0), get_global_id(1));		
	//Get the ant at this coordinate
	__global Ant *a = &pIn[coords.x + coords.y*640];
	int group = a->group;	
	int initialGroup = group;
	float value = a->value;
	
	
	
	//If im closer to my old group, switch
	if(a->lastGroup != -1 && fabs(a->lastGroupValue - value) <= switchBackThreshold){
		group =  a->lastGroup;
		a->groupTime = a->lastGroupTime;
		if(a->groupTime > a->lastGroupTime)
		{
			a->lastGroupTime = a->groupTime;
			a->lastGroupValue = a->lastValue;
			a->lastGroup = a->group;
		}
	}
	
	if(group == -1 && value > 0)
	{
		
		
		
		
		//Check neighbor to the right
		float friendDifference = 1;
		float foolDifference = 1;
		int friendsCount;					
		
		if(group == -1){	
			
			int2 neighborCoord = coords; 
			neighborCoord.x += 1;					
			if(neighborCoord.x < 640 ){	
				__global Ant * neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];							
				float neighborValue = neighbor->value;			
				int neighborGroup = neighbor->group;
				if(neighborGroup == initialGroup){
					friendsCount ++;
					if(neighborValue < friendDifference){
						friendDifference = neighborValue;
					}
				} else {
					if(neighborValue < foolDifference){
						foolDifference = neighborValue;
					}
				}	
				if(group == -1 && neighbor->group != -1 && fabs(neighborValue - value) < friendThreshold){
					group = neighborGroup;
				}
			}
		}
		
		//Check neighbor to the left
		if(group == -1)
		{
			int2 neighborCoord = coords; 
			neighborCoord.x -= 1;		
			
			if(neighborCoord.x > 0 ){	
				__global Ant * neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];							
				float neighborValue = neighbor->value;				
				int neighborGroup = neighbor->group;
				if(neighborGroup == initialGroup){
					friendsCount ++;
					if(neighborValue < friendDifference){
						friendDifference = neighborValue;
					}
				} else {
					if(neighborValue < foolDifference){
						foolDifference = neighborValue;
					}
				}	
				if(group == -1 && neighbor->group != -1 &&fabs(neighborValue - value) < friendThreshold){
					group = neighbor->group;	
				}
			}
		}
		
		//Check neighbor downwards
		if(group == -1)
		{
			int2 neighborCoord = coords; 
			neighborCoord.y += 1;		
			
			if(neighborCoord.y < 480 ){	
				__global Ant * neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];							
				float neighborValue = neighbor->value;				
				int neighborGroup = neighbor->group;
				if(neighborGroup == initialGroup){
					friendsCount ++;
					if(neighborValue < friendDifference){
						friendDifference = neighborValue;
					}
				} else {
					if(neighborValue < foolDifference){
						foolDifference = neighborValue;
					}
				}	
				if(group == -1 && neighbor->group != -1 &&fabs(neighborValue - value) < friendThreshold){
					group = neighbor->group;	
				}
			}
		}	
		
		//Check neighbor upwards
		if(group == -1)
		{
			int2 neighborCoord = coords; 
			neighborCoord.y -= 1;		
			
			if(neighborCoord.y > 0 ){
				__global Ant * neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];							
				float neighborValue = neighbor->value;				
				int neighborGroup = neighbor->group;
				if(neighborGroup == initialGroup){
					friendsCount ++;
					if(neighborValue < friendDifference){
						friendDifference = neighborValue;
					}
				} else {
					if(neighborValue < foolDifference){
						foolDifference = neighborValue;
					}
				}	
				if(group == -1 && neighbor->group != -1  &&fabs(neighborValue - value) < friendThreshold){
					group = neighbor->group;	
				}
			}
		}
		
		
		//Spawn new goups randomly
		if(group == -1 && spawnCoord.x == coords.x && spawnCoord.y == coords.y){
			group = sharedVariables[0]++;
		}
		
		
		//If this proccess gave a group, fill the area
		if(initialGroup == -1 && group != -1){
			{ //Fill to the right
				int2 neighborCoord = coords; 
				neighborCoord.x += 1;			
				if(neighborCoord.x < 640){						
					__global Ant * neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];		
					float nval = neighbor->value;
					float prevVal= value;					
					while(neighborCoord.x < 640 && neighbor->group == -1 && fabs(nval - prevVal) < friendThreshold){
						neighbor->group = group;						
						neighborCoord.x += 1;					
						neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];		
						prevVal = nval;
						nval = neighbor->value;
					}
				}
			}
			{ //Fill to the left
				int2 neighborCoord = coords; 
				neighborCoord.x -= 1;		
				if(neighborCoord.x > 0){	
					__global Ant * neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];		
					float nval = neighbor->value;
					float prevVal= value;					
					while(neighborCoord.x > 0 && neighbor->group == -1 && fabs(nval - prevVal) < friendThreshold){
						neighbor->group = group;						
						neighborCoord.x -= 1;					
						neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];		
						prevVal = nval;
						nval = neighbor->value;
					}
				}
			}
			{ //Fill downwards
				int2 neighborCoord = coords; 
				neighborCoord.y += 1;			
				if(neighborCoord.y < 480){						
					__global Ant * neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];		
					float nval = neighbor->value;
					float prevVal= value;					
					while(neighborCoord.y < 480 && neighbor->group == -1 && fabs(nval - prevVal) < friendThreshold){
						neighbor->group = group;						
						neighborCoord.y += 1;					
						neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];		
						prevVal = nval;
						nval = neighbor->value;
					}
				}
			}
			{ //Fill upwards
				int2 neighborCoord = coords; 
				neighborCoord.y -= 1;		
				if(neighborCoord.y > 0){						
					__global Ant * neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];		
					float nval = neighbor->value;
					float prevVal= value;					
					while(neighborCoord.y > 0 && neighbor->group == -1 && fabs(nval - prevVal) < friendThreshold){
						neighbor->group = group;						
						neighborCoord.y -= 1;					
						neighbor = &pIn[neighborCoord.x + neighborCoord.y*640];		
						prevVal = nval;
						nval = neighbor->value;
					}
				}
			}			
		}
		
		
	}
	//Update the groupvalue in the global memory if changed
	if(group != initialGroup){
		//a->groupTime = 0;
		a->group = group;
	}
}

//This kernel will update the debug image with colored groups
__kernel void postUpdate(read_only image2d_t srcImage, write_only image2d_t dstImage,  __global Ant* pIn, __global int * sharedVariables, const int2 spawnCoord){        
	int2 coords = (int2)(get_global_id(0), get_global_id(1));		
	__global Ant *a = &pIn[coords.x + coords.y*640];
	
	float4 outColor;	
	if(a->group == -1){
		outColor.x = 0;
		outColor.y = 0;
		outColor.z = 0;
		outColor.w = 1;
	} else {
		float r=1;
		float g=1;
		float b=1;
		HsvToRgb(a->group*34.76,1,1,&r,&g,&b);
		outColor.x = r;
		outColor.y = g;
		outColor.z = b;
		outColor.w = 1;	
	}
	
	write_imagef(dstImage, coords, outColor);
}
