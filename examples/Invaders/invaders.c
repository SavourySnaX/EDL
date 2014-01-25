/*
 * Space invaders implementation
 *
 *  Uses 8080 core built in EDL
 *  Rest is C for now
 *
 *
 *
 *
 *
 *
 *
 */

#include <GLFW/glfw3.h>
#include <GL/glext.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void DrawTiming(unsigned char* buffer,unsigned int width);

uint16_t PinGetPIN_A();
uint8_t PinGetPIN_D();
void PinSetPIN_D(uint8_t);
void PinSetPIN_READY(uint8_t);
void PinSetO1(uint8_t);
void PinSetO2(uint8_t);
uint8_t PinGetPIN_SYNC();
uint8_t PinGetPIN__WR();
void PinSetINT(uint8_t);
void PinSetRESET(uint8_t);

// Because certain pin's on the cpu are no longer readable external, the following buffers are used so that the timing capture will still work
uint8_t PIN_BUFFER_READY=0;
uint8_t PIN_BUFFER_RESET=0;
uint8_t PIN_BUFFER_O1=0;
uint8_t PIN_BUFFER_O2=0;
uint8_t PIN_BUFFER_INT=0;
uint8_t PIN_BUFFER_HOLD=0;

#define	SYNC_FETCH		(0xA2)
#define SYNC_MEM_READ		(0x82)
#define SYNC_MEM_WRITE		(0x00)
#define SYNC_STACK_READ		(0x86)
#define SYNC_STACK_WRITE	(0x04)
#define SYNC_INPUT		(0x42)
#define SYNC_OUTPUT		(0x10)
#define SYNC_HALT_ACK		(0x8A)
#define SYNC_INT_ACK		(0x23)

// Step 1. Memory

//0000 - 07FF	- invaders.h
//0800 - 0FFF	- invaders.g
//1000 - 17FF	- invaders.f
//1800 - 1FFF	- invaders.e

unsigned char Rom[0x2000];	// rom
unsigned char Ram[0x2000];

unsigned char RamCmp[0x2000];

int LoadRom(unsigned int address,unsigned int size,const char* fname)
{
	unsigned int readFileSize=0;
	FILE* inFile = fopen(fname,"rb");
	if (!inFile || size != (readFileSize = fread(&Rom[address],1,size,inFile)))
	{
		printf("Failed to open rom : %s - %d/%d",fname,readFileSize,size);
		return 1;
	}
	fclose(inFile);
	return 0;
}


int InitialiseMemory()
{
	if (LoadRom(0x0000,0x0800,"roms/invaders/invaders.h"))
		return 1;
	if (LoadRom(0x0800,0x0800,"roms/invaders/invaders.g"))
		return 1;
	if (LoadRom(0x1000,0x0800,"roms/invaders/invaders.f"))
		return 1;
	if (LoadRom(0x1800,0x0800,"roms/invaders/invaders.e"))
		return 1;

	// Blank video and ram for now - real hardware probably doesn't do this though!
	memset(&Ram[0],0,256*32);
	memset(&RamCmp[0],0,256*32);
	return 0;
}

void SaveScreen()
{
	int x,y,go=0;
	unsigned char zero=0;
	unsigned char all=255;
	FILE* screen=NULL;
	char filename[256];
	static int magic=0;
	
	for (y=32;y<256;y++)
	{
		for (x=0;x<32;x++)
		{
			if (Ram[y*32+x]!=RamCmp[y*32+x])
			{
				go=1;
			}
			RamCmp[y*32+x]=Ram[y*32+x];
		}
	}

	if (!go)
		return;

	sprintf(filename,"screens/out%05d.raw",magic++);
	screen = fopen(filename,"wb");
	for (y=32;y<256;y++)
	{
		for (x=0;x<32;x++)
		{
			unsigned char mask=0x01;
			while (mask)
			{
				if (Ram[y*32 + x]&mask)
				{
					fwrite(&all,1,1,screen);
				}
				else
				{
					fwrite(&zero,1,1,screen);
				}
				mask<<=1;
			}
		}
	}

	fclose(screen);
}
	
int masterClock=0;
int pixelClock=0;
int cpuClock=0;

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

void Disassemble(unsigned int address);

unsigned char NEXTINT;

unsigned char HandleIOPortRead(unsigned char port)
{
	unsigned char res=0;
	switch (port)
	{
	case 1:		// -|Player1Right|Player1Left|Player1Fire|-|1PlayerButton|2PlayerButton|Coin
		if (KeyDown('0'))
		{
			res|=0x01;
		}
		if (KeyDown('1'))
		{
			res|=0x04;
		}
		if (KeyDown(' '))
		{
			res|=0x10;
		}
		if (KeyDown(GLFW_KEY_LEFT))
		{
			res|=0x20;
		}
		if (KeyDown(GLFW_KEY_RIGHT))
		{
			res|=0x40;
		}
		return res;
	case 2:		// ShowCoinInfo|Player2Right|Player2Left|Player2Fire|-|Easy|NumShips(last 2 bits)
		return 0;
	case 3:
		return PinGetOUTPUTS();
	default:
		printf("Reading from unhandled IO Port %02X\n",port);
		break;
	}
		
	return 0x00;
}

void HandleIOPortWrite(unsigned char port,unsigned char data)
{
	switch (port)
	{
	case 2:
		PinSetINPUTS(data);
		PinSetLATCH_SHIFT(0);
		PinSetLATCH_SHIFT(1);
		break;
	case 3:				// Sound effects
		break;
	case 4:
		PinSetINPUTS(data);
		PinSetLATCH_DATA(0);
		PinSetLATCH_DATA(1);
		break;
	case 5:				// Sound effects
		break;
	case 6:				// Watchdog
		break;
	default:
		printf("Writing to unhandled IO Port %02X <- %02X\n",port,data);
		break;
	}

}

int dumpInstruction=0;

int g_instructionStep=0;
int g_traceStep=0;

int MEM_Handler()
{
	int keepGoing=g_traceStep^1;
	static unsigned char SYNC_LATCH;
	static int SYNC_A_LATCH;
	static int outOfRange=0;
	static int NewInstructionBegun=-1;

	// Watch for SYNC pulse and TYPE and latch them

	if (PinGetPIN_SYNC())
	{
		SYNC_LATCH=PinGetPIN_D();
		if (SYNC_LATCH==SYNC_FETCH)
		{
			SYNC_A_LATCH=PinGetPIN_A();
			NewInstructionBegun=2;
			if (dumpInstruction>0)
			{
				dumpInstruction--;
				outOfRange=1;
			}
			else
			{
				outOfRange=0;
			}
		}
	}

	if (NewInstructionBegun>-1)
	{
		if (NewInstructionBegun==0)
		{
			if (outOfRange)
				Disassemble(SYNC_A_LATCH);
			if (isBreakpoint(SYNC_A_LATCH))
				g_instructionStep=1;
			if (g_instructionStep)
				keepGoing=0;
		}
		NewInstructionBegun--;
	}

	// CPU INPUT expects data to be available on T2 state so we can do that work on the PIN_SYNC itself
	// Assume memory has no latency
	if (PinGetPIN_SYNC())
	{
		if (SYNC_LATCH==SYNC_FETCH || SYNC_LATCH==SYNC_STACK_READ || SYNC_LATCH==SYNC_MEM_READ)
		{
			if (PinGetPIN_A()>0x3FFF)
			{
				printf("Out of bounds read : %04X | masterClock : %d\n",PinGetPIN_A(),masterClock);
			}
			if ((PinGetPIN_A()&0x2000) == 0x2000)
			{
				PinSetPIN_D(Ram[PinGetPIN_A()&0x1FFF]);
			}
			else
			{
				PinSetPIN_D(Rom[PinGetPIN_A()&0x1FFF]);
			}
			if (dumpInstruction>0)
			{
				printf("Reading : %04X - %02X\n",PinGetPIN_A(),PinGetPIN_D());
			}
			PinSetPIN_READY(1);
			PIN_BUFFER_READY=1;
		}
		else if (SYNC_LATCH==SYNC_STACK_WRITE || SYNC_LATCH==SYNC_MEM_WRITE || SYNC_LATCH==SYNC_OUTPUT)
		{
			PinSetPIN_READY(1);
			PIN_BUFFER_READY=1;
		}
		else if (SYNC_LATCH==SYNC_INPUT)
		{
			PinSetPIN_D(HandleIOPortRead(PinGetPIN_A()&0xFF));
			PinSetPIN_READY(1);
			PIN_BUFFER_READY=1;
		}
		else if (SYNC_LATCH==SYNC_INT_ACK)
		{
			PinSetPIN_D(NEXTINT);
		}
		else
		{
			printf("Error unknown sync state!!! PIN_D = %02X\n",PinGetPIN_D());
			exit(12);
		}
	}
	
	// CPU OUTPUT expects device to have signalled readyness to capture at state T2, but capture occurs at T3 (when _WR is low)
	if (PinGetPIN__WR() == 0)
	{
		if (SYNC_LATCH==SYNC_STACK_WRITE || SYNC_LATCH==SYNC_MEM_WRITE)
		{
			if ((PinGetPIN_A()&0x2000)==0x2000)
			{
				Ram[PinGetPIN_A()&0x1FFF]=PinGetPIN_D();
			}
		}
		else if (SYNC_LATCH==SYNC_OUTPUT)
		{
			HandleIOPortWrite(PinGetPIN_A()&0xFF,PinGetPIN_D());
		}
	}

	return keepGoing;
}

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	256

#define TIMING_WIDTH	680
#define TIMING_HEIGHT	400

#define HEIGHT	256
#define	WIDTH	256

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define TIMING_WINDOW		1
#define REGISTER_WINDOW		2

GLFWwindow* windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

void ShowScreen(int windowNum,int w,int h)
{
	if (windowNum==MAIN_WINDOW)
	{
		int x,y,b;
		unsigned char* outputTexture = videoMemory[windowNum];
		for (y=32;y<256;y++)
		{
			for (x=0;x<32;x++)
			{
				unsigned char mask=0x01;
				for (b=0;b<8;b++)
				{
					if (Ram[y*32 + x]&mask)
					{
						outputTexture[(x*8+b+y*256)*4+0]=0xFF;
						outputTexture[(x*8+b+y*256)*4+1]=0xFF;
						outputTexture[(x*8+b+y*256)*4+2]=0xFF;
					}
					else
					{
						outputTexture[(x*8+b+y*256)*4+0]=0x0;
						outputTexture[(x*8+b+y*256)*4+1]=0x0;
						outputTexture[(x*8+b+y*256)*4+2]=0x0;
					}
					mask<<=1;
				}
			}
		}
	}
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	if (windowNum==MAIN_WINDOW)		// Invaders needs rotating
	{
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f((-1.0f * 0) - (1.0f*1.f), (1.0f*0) + (-1.f*1.0f));

		glTexCoord2f(0.0f, h);
		glVertex2f((-1.0f * 0) - (-1.0f*1.f), (-1.0f*0) + (-1.f*1.0f));

		glTexCoord2f(w, h);
		glVertex2f((1.0f * 0) - (-1.0f*1.f), (-1.0f*0) + (1.f*1.0f));

		glTexCoord2f(w, 0.0f);
		glVertex2f((1.0f * 0) - (1.0f*1.f), (1.0f*0) + (1.f*1.0f));
	}
	else
	{
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(-1.0f,1.0f);

		glTexCoord2f(0.0f, h);
		glVertex2f(-1.0f, -1.0f);

		glTexCoord2f(w, h);
		glVertex2f(1.0f, -1.0f);

		glTexCoord2f(w, 0.0f);
		glVertex2f(1.0f, 1.0f);
	}
	glEnd();
	
	glFlush();
}

void setupGL(int windowNum,int w, int h) 
{
	videoTexture[windowNum] = windowNum+1;
	videoMemory[windowNum] = (unsigned char*)malloc(w*h*sizeof(unsigned int));
	memset(videoMemory[windowNum],0,w*h*sizeof(unsigned int));
	//Tell OpenGL how to convert from coordinates to pixel values
	glViewport(0, 0, w, h);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glClearColor(1.0f, 0.f, 1.0f, 1.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);

	//	glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, 0, NULL);

	//	glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_CACHED_APPLE);
	//	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA, w,
			h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);

	glDisable(GL_DEPTH_TEST);
}

void restoreGL(int windowNum,int w, int h) 
{
	//Tell OpenGL how to convert from coordinates to pixel values
	glViewport(0, 0, w, h);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glClearColor(1.0f, 0.f, 1.0f, 1.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glDisable(GL_DEPTH_TEST);
}

unsigned char keyArray[512*3];

int KeyDown(int key)
{
	return keyArray[key*3+1]==GLFW_PRESS;
}

int CheckKey(int key)
{
	return keyArray[key*3+2];
}

void ClearKey(int key)
{
	keyArray[key*3+2]=0;
}

void kbHandler( GLFWwindow* window, int key, int scan, int action, int mods )		/* At present ignores which window, will add per window keys later */
{
	keyArray[key*3 + 0]=keyArray[key*3+1];
	if (action==GLFW_RELEASE)
	{
		keyArray[key*3 + 1]=GLFW_RELEASE;
	}
	else
	{
		keyArray[key*3 + 1]=GLFW_PRESS;
	}
	keyArray[key*3 + 2]|=(keyArray[key*3+0]==GLFW_RELEASE)&&(keyArray[key*3+1]==GLFW_PRESS);
}

int mouseWheelDelta=0;

int bTimingEnabled=1;
int bRegisterEnabled=1;

void mwHandler( GLFWwindow* window,double posx,double posy)
{
	mouseWheelDelta=(int)posy;
	UpdateTiming(mouseWheelDelta);
}

int main(int argc,char**argv)
{
	double	atStart,now,remain;

	/// Initialize GLFW 
	glfwInit(); 

	// Open registers OpenGL window 
	if( !(windows[REGISTER_WINDOW]=glfwCreateWindow( REGISTER_WIDTH, REGISTER_HEIGHT, "cpu",NULL,NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[REGISTER_WINDOW],600,740);

	glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
	setupGL(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);

	// Open timing OpenGL window 
	if( !(windows[TIMING_WINDOW]=glfwCreateWindow( TIMING_WIDTH, TIMING_HEIGHT, "timing",NULL,NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[TIMING_WINDOW],600,300);

	glfwMakeContextCurrent(windows[TIMING_WINDOW]);
	setupGL(TIMING_WINDOW,TIMING_WIDTH,TIMING_HEIGHT);

	// Open invaders OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwCreateWindow( WIDTH, HEIGHT, "invaders",NULL,NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC

	glfwSetKeyCallback(windows[MAIN_WINDOW],kbHandler);
	glfwSetScrollCallback(windows[TIMING_WINDOW],mwHandler);

	atStart=glfwGetTime();
	//////////////////

	if (InitialiseMemory())
		return -1;
	
	PinSetRESET(1);
	PIN_BUFFER_RESET=1;
	PinSetO1(1);			// Run with reset high for a few cycles to perform a full cpu reset
	PinSetO1(0);
	PinSetO2(1);
	PinSetO2(0);
	PinSetO1(1);
	PinSetO1(0);
	PinSetO2(1);
	PinSetO2(0);
	PinSetO1(1);
	PinSetO1(0);
	PinSetO2(1);
	PinSetO2(0);
	PinSetRESET(0);			// RESET CPU
	PIN_BUFFER_RESET=0;

	//dumpInstruction=100000;

	int stopTheClock=0;
	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESCAPE))
	{
		
		if (!stopTheClock)
		{
			masterClock++;
			if ((masterClock%4)==0)
				pixelClock++;

			if ((masterClock%10)==0)
			{
								// I8080 emulation works off positive edge trigger. So we need to supply the same sort of
								// clock.
				PIN_BUFFER_O2=0;
				PIN_BUFFER_O1=1;
				PinSetO1(1);		// Execute a cpu step
				if (bTimingEnabled)
					RecordPins();
				PIN_BUFFER_O1=0;
				PinSetO1(0);
				if (bTimingEnabled)
					RecordPins();
				PIN_BUFFER_O2=1;
				PinSetO2(1);
				if (bTimingEnabled)
					RecordPins();
				PIN_BUFFER_O2=0;
				PinSetO2(0);

				if (!MEM_Handler())
				{
					stopTheClock=1;
				}
				if (bTimingEnabled)
					RecordPins();

				PinSetINT(0);		// clear interrupt state
				PIN_BUFFER_INT=0;
				cpuClock++;
			}
			if (pixelClock==30432+10161)		// Based on 19968000 Mhz master clock + mame notes
			{
				NEXTINT=0xCF;
				PinSetINT(1);
				PIN_BUFFER_INT=1;
			}
			if (pixelClock==71008+10161)
			{
				NEXTINT=0xD7;
				PinSetINT(1);
				PIN_BUFFER_INT=1;
			}
		}
		if (pixelClock>=83200 || stopTheClock)
		{
			if (pixelClock>=83200)
				pixelClock=0;

			if (glfwWindowShouldClose(windows[TIMING_WINDOW]))
			{
				bTimingEnabled=0;
				glfwHideWindow(windows[TIMING_WINDOW]);
			}
			if (glfwWindowShouldClose(windows[REGISTER_WINDOW]))
			{
				bRegisterEnabled=0;
				glfwHideWindow(windows[REGISTER_WINDOW]);
			}

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers(windows[MAIN_WINDOW]);
				
			if (bTimingEnabled)
			{
				glfwMakeContextCurrent(windows[TIMING_WINDOW]);
				DrawTiming(videoMemory[TIMING_WINDOW],TIMING_WIDTH);
				ShowScreen(TIMING_WINDOW,TIMING_WIDTH,TIMING_HEIGHT);
				glfwSwapBuffers(windows[TIMING_WINDOW]);
			}
			if (bRegisterEnabled)
			{
				glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
				DrawRegister(videoMemory[REGISTER_WINDOW],REGISTER_WIDTH);
				ShowScreen(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);
				glfwSwapBuffers(windows[REGISTER_WINDOW]);
			}
        
			glfwPollEvents();
			
			g_traceStep=0;
			if (CheckKey(GLFW_KEY_PAUSE))
			{
				g_instructionStep^=1;
				if (stopTheClock && !g_instructionStep)
					stopTheClock=0;
				ClearKey(GLFW_KEY_PAUSE);
			}
			if (stopTheClock && CheckKey('S'))
			{
				stopTheClock=0;
				ClearKey('S');
			}
			if (stopTheClock && CheckKey('T'))
			{
				stopTheClock=0;
				g_traceStep=1;
				ClearKey('T');
			}

			now=glfwGetTime();

			remain = now-atStart;

			while ((remain<0.02f))
			{
				now=glfwGetTime();

				remain = now-atStart;
			}
			atStart=glfwGetTime();
		}
	}
	
	return 0;

}

