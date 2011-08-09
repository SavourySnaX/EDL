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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <GL/glfw.h>
#include <GL/glext.h>

extern unsigned short PIN_A;
extern unsigned char PIN_D;
extern unsigned char PIN_READY;
extern unsigned char PIN_RESET;
extern unsigned char PIN_SYNC;
extern unsigned char PIN__WR;

void O1(void);
void O2(void);
void RESET(void);
void INT(void);

extern unsigned char	SYNC_FETCH;
extern unsigned char	SYNC_MEM_READ;
extern unsigned char	SYNC_MEM_WRITE;
extern unsigned char	SYNC_STACK_READ;
extern unsigned char	SYNC_STACK_WRITE;
extern unsigned char	SYNC_INPUT;
extern unsigned char	SYNC_OUTPUT;
extern unsigned char	SYNC_INT_ACK;
extern unsigned char	SYNC_HALT_ACK;
extern unsigned char	SYNC_INT_ACK_HALTED;

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
	if (LoadRom(0x0000,0x0800,"invaders/invaders.h"))
		return 1;
	if (LoadRom(0x0800,0x0800,"invaders/invaders.g"))
		return 1;
	if (LoadRom(0x1000,0x0800,"invaders/invaders.f"))
		return 1;
	if (LoadRom(0x1800,0x0800,"invaders/invaders.e"))
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

unsigned int PORT4_LATCH;
unsigned int PORT2_LATCH;

unsigned char HandleIOPortRead(unsigned char port)
{
	unsigned char res=0;
	switch (port)
	{
	case 1:		// -|Player1Right|Player1Left|Player1Fire|-|1PlayerButton|2PlayerButton|Coin
		res=0;
		if (CheckKey('0'))
		{
			res|=0x01;
			ClearKey('0');
		}
		if (CheckKey('1'))
		{
			res|=0x04;
			ClearKey('1');
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
		return (PORT4_LATCH << PORT2_LATCH)>>8;
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
		PORT2_LATCH=data;
		break;
	case 3:				// Sound effects
		break;
	case 4:
		PORT4_LATCH>>=8;
		PORT4_LATCH&=0x000000FF;
		PORT4_LATCH|=data<<8;
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

void MEM_Handler()
{
	static unsigned char SYNC_LATCH;
	static int SYNC_A_LATCH;
	static int outOfRange=0;
	static int disassemblein=-1;

	// Watch for SYNC pulse and TYPE and latch them

	if (PIN_SYNC)
	{
		SYNC_LATCH=PIN_D;
		if (SYNC_LATCH==SYNC_FETCH)
		{
			SYNC_A_LATCH=PIN_A;
			disassemblein=2;
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

	if (disassemblein>-1)
	{
		if (disassemblein==0)
		{
			if (outOfRange)
			Disassemble(SYNC_A_LATCH);
		}
		disassemblein--;
	}

	// CPU INPUT expects data to be available on T2 state so we can do that work on the PIN_SYNC itself
	// Assume memory has no latency
	if (PIN_SYNC)
	{
		if (SYNC_LATCH==SYNC_FETCH || SYNC_LATCH==SYNC_STACK_READ || SYNC_LATCH==SYNC_MEM_READ)
		{
			if (PIN_A>0x3FFF)
			{
				printf("Out of bounds read : %04X | masterClock : %d\n",PIN_A,masterClock);
			}
			if ((PIN_A&0x2000) == 0x2000)
			{
				PIN_D=Ram[PIN_A&0x1FFF];
			}
			else
			{
				PIN_D=Rom[PIN_A&0x1FFF];
			}
			if (dumpInstruction>0)
			{
				printf("Reading : %04X - %02X\n",PIN_A,PIN_D);
			}
			PIN_READY=1;
		}
		else if (SYNC_LATCH==SYNC_STACK_WRITE || SYNC_LATCH==SYNC_MEM_WRITE || SYNC_LATCH==SYNC_OUTPUT)
		{
			PIN_READY=1;
		}
		else if (SYNC_LATCH==SYNC_INPUT)
		{
			PIN_D=HandleIOPortRead(PIN_A&0xFF);
			PIN_READY=1;
		}
		else if (SYNC_LATCH==SYNC_INT_ACK)
		{
			PIN_D=NEXTINT;
		}
		else
		{
			printf("Error unknown sync state!!! PIN_D = %02X\n",PIN_D);
			exit(12);
		}
	}
	
	// CPU OUTPUT expects device to have signalled readyness to capture at state T2, but capture occurs at T3 (when _WR is low)
	if (PIN__WR == 0)
	{
		if (SYNC_LATCH==SYNC_STACK_WRITE || SYNC_LATCH==SYNC_MEM_WRITE)
		{
			if ((PIN_A&0x2000)==0x2000)
			{
				Ram[PIN_A&0x1FFF]=PIN_D;
			}
		}
		else if (SYNC_LATCH==SYNC_OUTPUT)
		{
			HandleIOPortWrite(PIN_A&0xFF,PIN_D);
		}
	}
}

#define HEIGHT	256
#define	WIDTH	256

unsigned char videoMemory[WIDTH*HEIGHT*sizeof(unsigned int)];

void ShowScreen() 
{
	int x,y,b;
	for (y=32;y<256;y++)
	{
		for (x=0;x<32;x++)
		{
			unsigned char mask=0x01;
			for (b=0;b<8;b++)
			{
				if (Ram[y*32 + x]&mask)
				{
					videoMemory[(x*8+b+y*256)*4+0]=0xFF;
					videoMemory[(x*8+b+y*256)*4+1]=0xFF;
					videoMemory[(x*8+b+y*256)*4+2]=0xFF;
				}
				else
				{
					videoMemory[(x*8+b+y*256)*4+0]=0x0;
					videoMemory[(x*8+b+y*256)*4+1]=0x0;
					videoMemory[(x*8+b+y*256)*4+2]=0x0;
				}
				mask<<=1;
			}
		}
	}
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, 1);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, WIDTH, HEIGHT, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f((-1.0f * 0) - (1.0f*1.f), (1.0f*0) + (-1.f*1.0f));

	glTexCoord2f(0.0f, HEIGHT);
	glVertex2f((-1.0f * 0) - (-1.0f*1.f), (-1.0f*0) + (-1.f*1.0f));

	glTexCoord2f(WIDTH, HEIGHT);
	glVertex2f((1.0f * 0) - (-1.0f*1.f), (-1.0f*0) + (1.f*1.0f));

	glTexCoord2f(WIDTH, 0.0f);
	glVertex2f((1.0f * 0) - (1.0f*1.f), (1.0f*0) + (1.f*1.0f));
	
	glEnd();
	
	glFlush();
}

void setupGL(int w, int h) 
{
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
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, 1);
	
//	glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, 0, NULL);
	
//	glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_CACHED_APPLE);
//	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA, WIDTH,
				 HEIGHT, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory);
	
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

void GLFWCALL kbHandler( int key, int action )
{
	keyArray[key*3 + 0]=keyArray[key*3+1];
	keyArray[key*3 + 1]=action;
	keyArray[key*3 + 2]|=(keyArray[key*3+0]==GLFW_RELEASE)&&(keyArray[key*3+1]==GLFW_PRESS);
}

int main(int argc,char**argv)
{
	double	atStart,now,remain;

	/// Initialize GLFW 
	glfwInit(); 
	// Open an OpenGL window 
	if( !glfwOpenWindow( WIDTH, HEIGHT, 0,0,0,0,0,0, GLFW_WINDOW ) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowTitle("Invaders");
	glfwSetWindowPos(300,300);
	
	setupGL(WIDTH,HEIGHT);	

	glfwSwapInterval(0);			// Disable VSYNC

	glfwSetKeyCallback(kbHandler);

	atStart=glfwGetTime();
	//////////////////

	if (InitialiseMemory())
		return -1;
	
	PIN_RESET=1;
	RESET();
	O1();
	O2();
	O1();
	O2();
	O1();
	O2();
	PIN_RESET=0;
	RESET();			// RESET CPU

	//dumpInstruction=100000;

	while (!glfwGetKey(GLFW_KEY_ESC))
	{
		masterClock++;
		if ((masterClock%4)==0)
			pixelClock++;

		if ((masterClock%10)==0)
		{
			O1();			// Execute a cpu step
			O2();

			MEM_Handler();		// 

			cpuClock++;
		}
		if (pixelClock==30432)		// Based on 19968000 Mhz master clock + mame notes
		{
			NEXTINT=0xCF;
			INT();
		}
		if (pixelClock==71008)
		{
			NEXTINT=0xD7;
			INT();
		}
		if (pixelClock>=83200)
		{
			pixelClock=0;
			ShowScreen();

			glfwSwapBuffers();
			
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

