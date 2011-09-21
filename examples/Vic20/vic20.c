/*
 * VIC20 implementation
 *
 *
 *
 *
 *
 *
 *
 */

#include <GL/glfw3.h>
#include <GL/glext.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

uint16_t PinGetPIN_AB();
uint8_t PinGetPIN_DB();
void PinSetPIN_DB(uint8_t);
void PinSetPIN_O0(uint8_t);
uint8_t PinGetPIN_SYNC();
uint8_t PinGetPIN_RW();
void PinSetPIN__IRQ(uint8_t);
void PinSetPIN__RES(uint8_t);

// Step 1. Memory

unsigned char CRom[0x1000];
unsigned char BRom[0x2000];
unsigned char KRom[0x2000];

unsigned char RamLo[0x400];
unsigned char RamHi[0xE00];

unsigned char CRam[0x200];
unsigned char SRam[0x200];

int LoadRom(unsigned char* rom,unsigned int size,const char* fname)
{
	unsigned int readFileSize=0;
	FILE* inFile = fopen(fname,"rb");
	if (!inFile || size != (readFileSize = fread(rom,1,size,inFile)))
	{
		printf("Failed to open rom : %s - %d/%d",fname,readFileSize,size);
		return 1;
	}
	fclose(inFile);
	return 0;
}


int InitialiseMemory()
{
	if (LoadRom(CRom,0x1000,"roms/901460-03.ud7"))
		return 1;
	if (LoadRom(BRom,0x2000,"roms/901486-01.ue11"))
		return 1;
	if (LoadRom(KRom,0x2000,"roms/901486-07.ue12"))
		return 1;

	return 0;
}

uint8_t GetByte(uint16_t addr)
{
	if (addr<1024)
		return RamLo[addr];
	if (addr<4096)
		return 0xFF;
	if (addr<7680)
		return RamHi[addr-4096];
	if (addr<8192)
		return SRam[addr-7680];
	if (addr<32768)
		return 0xFF;
	if (addr<36864)
		return CRom[addr-32768];
	if (addr<38400)
	{
		// Various expansions
		printf("Attempt to acccess : %04X\n",addr);
		return 0xFF;
	}
	if (addr<38912)
		return CRam[addr-38400]&0x0F;
	if (addr<49152)
		return 0xFF;
	if (addr<57344)
		return BRom[addr-49152];
	return KRom[addr-57344];
}

void SetByte(uint16_t addr,uint8_t byte)
{
	if (addr<1024)
		RamLo[addr]=byte;
	if (addr<4096)
		return;
	if (addr<7680)
		RamHi[addr-4096]=byte;
	if (addr<8192)
		SRam[addr-7680]=byte;
	if (addr<32768)
		return;
	if (addr<36864)
		return;
	if (addr<38400)
	{
		// Various expansions
		printf("WRITE Attempt to acccess :%02X %04X\n",byte,addr);
		return;
	}
	if (addr<38912)
		CRam[addr-38400]=byte&0x0F;
	if (addr<49152)
		return;
	if (addr<57344)
		return;
	return;
}


int masterClock=0;
int pixelClock=0;
int cpuClock=0;

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

extern uint8_t *DIS_[256];

extern uint8_t	A;
extern uint8_t	X;
extern uint8_t	Y;
extern uint16_t	SP;
extern uint16_t	PC;
extern uint8_t	P;

void DUMP_REGISTERS()
{
	printf("--------\n");
	printf("FLAGS = N  V  -  B  D  I  Z  C\n");
	printf("        %s  %s  %s  %s  %s  %s  %s  %s\n",
			P&0x80 ? "1" : "0",
			P&0x40 ? "1" : "0",
			P&0x20 ? "1" : "0",
			P&0x10 ? "1" : "0",
			P&0x08 ? "1" : "0",
			P&0x04 ? "1" : "0",
			P&0x02 ? "1" : "0",
			P&0x01 ? "1" : "0");
	printf("A = %02X\n",A);
	printf("X = %02X\n",X);
	printf("Y = %02X\n",Y);
	printf("SP= %04X\n",SP);
	printf("--------\n");
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

	uint8_t byte = GetByte(address);
	if (byte>realLength)
	{
		sprintf(temporaryBuffer,"UNKNOWN OPCODE");
		return temporaryBuffer;
	}
	else
	{
		const char* mnemonic=table[byte];
		const char* sPtr=mnemonic;
		char* dPtr=temporaryBuffer;
		int counting = 0;
		int doingDecode=0;

		if (sPtr==NULL)
		{
			sprintf(temporaryBuffer,"UNKNOWN OPCODE");
			return temporaryBuffer;
		}

		while (*sPtr)
		{
			if (!doingDecode)
			{
				if (*sPtr=='%')
				{
					doingDecode=1;
					if (*(sPtr+1)=='$')
						sPtr++;
				}
				else
				{
					*dPtr++=*sPtr;
				}
			}
			else
			{
				char *tPtr=sprintBuffer;
				int negOffs=1;
				if (*sPtr=='-')
				{
					sPtr++;
					negOffs=-1;
				}
				int offset=(*sPtr-'0')*negOffs;
				sprintf(sprintBuffer,"%02X",GetByte(address+offset));
				while (*tPtr)
				{
					*dPtr++=*tPtr++;
				}
				doingDecode=0;
				counting++;
			}
			sPtr++;
		}
		*dPtr=0;
		*count=counting;
	}
	return temporaryBuffer;
}

int Disassemble(unsigned int address,int registers)
{
	int a;
	int numBytes=0;
	const char* retVal = decodeDisasm(DIS_,address,&numBytes,255);

	if (strcmp(retVal,"UNKNOWN OPCODE")==0)
	{
		printf("UNKNOWN AT : %04X\n",address);
		for (a=0;a<numBytes+1;a++)
		{
			printf("%02X ",GetByte(address+a));
		}
		printf("\n");
		exit(-1);
	}

	if (registers)
	{
		DUMP_REGISTERS();
	}
	printf("%04X :",address);

	for (a=0;a<numBytes+1;a++)
	{
		printf("%02X ",GetByte(address+a));
	}
	for (a=0;a<8-(numBytes+1);a++)
	{
		printf("   ");
	}
	printf("%s\n",retVal);

	return numBytes+1;
}


unsigned char NEXTINT;

int dumpInstruction=0;

int g_instructionStep=0;
int g_traceStep=0;

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	256

#define TIMING_WIDTH	680
#define TIMING_HEIGHT	400

#define HEIGHT	22*8
#define	WIDTH	22*8

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define TIMING_WINDOW		1
#define REGISTER_WINDOW		2

GLFWwindow windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

void ShowScreen(int windowNum,int w,int h)
{
#if 0
	if (windowNum==MAIN_WINDOW)
	{
		int x,y,b;
		unsigned char* outputTexture = videoMemory[windowNum];
		for (y=0;y<22;y++)
		{
			for (x=0;x<22;x++)
			{
				unsigned char index = SRam[x+y*22];
				
				if (index<0)
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
#endif
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

void kbHandler( GLFWwindow window, int key, int action )		/* At present ignores which window, will add per window keys later */
{
	keyArray[key*3 + 0]=keyArray[key*3+1];
	keyArray[key*3 + 1]=action;
	keyArray[key*3 + 2]|=(keyArray[key*3+0]==GLFW_RELEASE)&&(keyArray[key*3+1]==GLFW_PRESS);
}

int main(int argc,char**argv)
{
	double	atStart,now,remain;
	uint16_t bp;

	/// Initialize GLFW 
	glfwInit(); 

	// Open screen OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwOpenWindow( WIDTH, HEIGHT, GLFW_WINDOWED,"invaders",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC

	glfwSetKeyCallback(kbHandler);

	atStart=glfwGetTime();
	//////////////////

	if (InitialiseMemory())
		return -1;
	
#if 0
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
#endif
	//dumpInstruction=100000;

	PC=GetByte(0xFFFC);
	PC|=GetByte(0xFFFD)<<8;

	bp = GetByte(0xc000);
	bp|=GetByte(0xC001)<<8;

	printf("%04X\n",bp);

	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
	{
		static int doDebug=0;

		PinSetPIN_O0(1);
		if (PinGetPIN_SYNC())
		{
			if (PinGetPIN_AB()==bp)
				doDebug=1;
			
			if (doDebug)
			{
				Disassemble(PinGetPIN_AB(),1);
				getch();
			}
		}
		if (PinGetPIN_RW())
		{
			uint16_t addr = PinGetPIN_AB();
			uint8_t  data = GetByte(addr);
			if (doDebug)
				printf("Getting : %02X @ %04X\n", data,PinGetPIN_AB());
			PinSetPIN_DB(data);
		}
		if (!PinGetPIN_RW())
		{
			if (doDebug)
				printf("Storing : %02X @ %04X\n", PinGetPIN_DB(),PinGetPIN_AB());
			SetByte(PinGetPIN_AB(),PinGetPIN_DB());
		}

		PinSetPIN_O0(0);

		pixelClock++;

		if (CheckKey(' '))
		{
			ClearKey(' ');
			{
				int x,y;
				for (y=0;y<22;y++)
				{
					for (x=0;x<22;x++)
					{
						printf("%02X ",SRam[x+y*22]);
					}
					printf("\n");
				}
			}
		}

#if 0
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
#endif
		if (pixelClock>=83200/* || stopTheClock*/)
		{
			if (pixelClock>=83200)
				pixelClock=0;

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
				
			glfwPollEvents();
			
			g_traceStep=0;
			
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

