#include <GL/glfw3.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "font.h"

unsigned short breakpoints[2][20]={
	{0x44E,0},
	{/*0xE853,*//*0xF556,*/0xfd27,0xfd86,0xfd58,0xF497,0xF3C0,0xF567}
	};		// first list main cpu, second is disk cpu
unsigned int numBreakpoints[2]={0,0};

unsigned short curAddresses[2][16];
int Offs[2]={0,0};

void ToggleBreakPoint(int chip)
{
	int a,c;

	for (a=0;a<numBreakpoints[chip];a++)
	{
		if (curAddresses[chip][Offs[chip]]==breakpoints[chip][a])
		{
			for (c=a;c<numBreakpoints[chip];c++)
			{
				breakpoints[chip][c]=breakpoints[chip][c+1];
			}
			numBreakpoints[chip]--;
			return;
		}
	}

	breakpoints[chip][numBreakpoints[chip]]=curAddresses[chip][Offs[chip]];
	numBreakpoints[chip]++;
}

int isBreakpoint(int chip,uint16_t address)
{
	int a;
	for (a=0;a<numBreakpoints[chip];a++)
	{
		if (address==breakpoints[chip][a])
		{
			return 1;
		}
	}
	return 0;
}

void DrawChar(unsigned char* buffer,unsigned int width,unsigned int x,unsigned int y, char c,unsigned char r,unsigned char g,unsigned char b)
{
	unsigned int xx,yy;
	unsigned char *fontChar=&FontData[c*6*8];
	
	x*=8;
	y*=8;
	
	for (yy=0;yy<8;yy++)
	{
		for (xx=0;xx<6;xx++)
		{
			buffer[(x+xx+1 + (y+yy)*width)*4+0]=(*fontChar)*b;
			buffer[(x+xx+1 + (y+yy)*width)*4+1]=(*fontChar)*g;
			buffer[(x+xx+1 + (y+yy)*width)*4+2]=(*fontChar)*r;
			fontChar++;
		}
	}
}

void PrintAt(unsigned char* buffer,unsigned int width,unsigned char r,unsigned char g,unsigned char b,unsigned int x,unsigned int y,const char *msg,...)
{
	static char tStringBuffer[32768];
	char *pMsg=tStringBuffer;
	va_list args;

	va_start (args, msg);
	vsprintf (tStringBuffer,msg, args);
	va_end (args);

	while (*pMsg)
	{
		DrawChar(buffer,width,x,y,*pMsg,r,g,b);
		x++;
		pMsg++;
	}
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength);

extern uint8_t *MAIN_DIS_[256];

extern uint8_t	MAIN_A;
extern uint8_t	MAIN_X;
extern uint8_t	MAIN_Y;
extern uint16_t	MAIN_SP;
extern uint16_t	MAIN_PC;
extern uint8_t	MAIN_P;

void DrawRegister(int chip,uint8_t *table[256],unsigned char* buffer,unsigned int width,uint16_t address,uint8_t (*GetMem)(uint16_t),const char *(*decode)(uint8_t *table[256],unsigned int address,int *count,int realLength))
{
	int a;
	unsigned int b;
	int numBytes=0;
	
	for (b=0;b<16;b++)
	{
		const char* retVal = decode(table,address,&numBytes,255);
		int opcodeLength=numBytes+1;
		unsigned char colR=255,colG=255,colB=255;

		curAddresses[chip][b]=address;
		if (isBreakpoint(chip,address))
		{
			colG=0;
			colB=0;
		}

		if (Offs[chip]==b)
		{
			PrintAt(buffer,width,colR,colG,colB,0,8+b,"%04X >",address);
		}
		else
		{
			PrintAt(buffer,width,colR,colG,colB,0,8+b,"%04X  ",address);
		}

		PrintAt(buffer,width,255,255,255,8,8+b,"            ");
		for (a=0;a<opcodeLength;a++)
		{
			PrintAt(buffer,width,colR,colG,colB,8+a*3,8+b,"%02X ",GetMem(address+a));
		}
		PrintAt(buffer,width,255,255,255,8+4*3,8+b,"            ");
		PrintAt(buffer,width,colR,colG,colB,8+3*3,8+b,"%s",retVal);

		address+=opcodeLength;
	}
}

void UpdateRegisterMain(GLFWwindow window)
{
	if (CheckKeyWindow(GLFW_KEY_DOWN,window))
	{
		Offs[0]++;
		if (Offs[0]>15)
		{
			Offs[0]=15;
		}
		ClearKey(GLFW_KEY_DOWN);
	}
	if (CheckKeyWindow(GLFW_KEY_UP,window))
	{
		Offs[0]--;
		if (Offs[0]<0)
		{
			Offs[0]=0;
		}
		ClearKey(GLFW_KEY_UP);
	}
	if (CheckKeyWindow(GLFW_KEY_SPACE,window))
	{
		ToggleBreakPoint(0);
		ClearKey(GLFW_KEY_SPACE);
	}
}

void DrawRegisterMain(unsigned char* buffer,unsigned int width,uint16_t address,uint8_t (*GetMem)(uint16_t))
{
	PrintAt(buffer,width,255,255,255,0,0,"FLAGS = N  V  -  B  D  I  Z  C");
	PrintAt(buffer,width,255,255,255,0,1,"        %s  %s  %s  %s  %s  %s  %s  %s",
		MAIN_P&0x80 ? "1" : "0",
		MAIN_P&0x40 ? "1" : "0",
		MAIN_P&0x20 ? "1" : "0",
		MAIN_P&0x10 ? "1" : "0",
		MAIN_P&0x08 ? "1" : "0",
		MAIN_P&0x04 ? "1" : "0",
		MAIN_P&0x02 ? "1" : "0",
		MAIN_P&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,2,"A   %02X",MAIN_A);
	PrintAt(buffer,width,255,255,255,0,3,"X   %02X",MAIN_X);
	PrintAt(buffer,width,255,255,255,0,4,"Y   %02X",MAIN_Y);
	PrintAt(buffer,width,255,255,255,0,5,"SP  %04X",MAIN_SP);

	DrawRegister(0,MAIN_DIS_,buffer,width,address,GetMem,decodeDisasm);
}

#define MAX_CAPTURE		(341*262*2*4*3)
#define MAX_PINS		(4)

unsigned char lohi[MAX_PINS][MAX_CAPTURE];

int bufferPosition=0;
uint64_t timeStretch=0x10000;
uint64_t timeOffset=0;

void RecordPin(int pinPos,uint8_t pinVal)
{
	if (pinPos>=3)
	{
		lohi[pinPos][bufferPosition]=pinVal;
	}
	else
		lohi[pinPos][bufferPosition]=pinVal&1;
}

void UpdatePinTick()
{
	bufferPosition++;
	if (bufferPosition>=MAX_CAPTURE)
		bufferPosition=0;
}

uint32_t cols[2]={0x00FFFFFF,0x00808080};

void DrawTimingA(unsigned char* buffer,unsigned int width)
{
	int a,b,c;
	uint64_t pulsepos;
	uint64_t pulsestart;
	uint32_t* argb=(uint32_t*)buffer;
/*
	pulsepos=(bufferPosition-(512*2-1))<<16;
	if ((pulsepos>>16)<0)
	{
		pulsepos+=MAX_CAPTURE<<16;
	}
	pulsestart=pulsepos;
*/
	pulsepos=bufferPosition<<16;
	pulsepos+=timeOffset;
	if ((pulsepos>>16)>MAX_CAPTURE)
	{
		pulsepos-=MAX_CAPTURE<<16;
	}
	pulsestart=pulsepos;
	// Clear graph space
	
	for (b=0;b<512*2;b++)
	{
		for (c=50;c<256+50;c++)
		{
			argb[80+b+c*width]=0;
		}
	}

	uint32_t level;
	for (a=3;a<MAX_PINS;a++)
	{
		pulsepos=pulsestart;
		uint32_t lastLevel=50+(255-(lohi[a][pulsepos>>16]));
		for (b=0;b<512*2;b++)
		{
			int c;
			int low;
			int high;

			level = 50+(255-(lohi[a][pulsepos>>16]));
			low=lastLevel;
			high=level;
			if (low>high)
			{
				low=level;
				high=lastLevel;
			}
			for (c=low;c<=high;c++)
			{
				argb[80+b+c*width]=cols[a-3];
			}

			pulsepos+=timeStretch;
			if ((pulsepos>>16)>=MAX_CAPTURE)
			{
				pulsepos=0;
			}

			lastLevel=level;
		}
	}
}

void DrawTiming(unsigned char* buffer,unsigned int width)
{
	int a,b;
	uint64_t pulsepos;
	uint64_t prevpulsepos;

	PrintAt(buffer,width,255,255,255,0,0,"CLK (CPU)");
	PrintAt(buffer,width,255,255,255,0,3,"M2  (CPU)");
	PrintAt(buffer,width,255,255,255,0,6,"DBE (PPU)");
//	PrintAt(buffer,width,255,255,255,0,9,"CLK (DISK)");
//	PrintAt(buffer,width,255,255,255,0,12,"DAT (DISK)");
//	PrintAt(buffer,width,255,255,255,0,15,"ATN (DISK)");
//	PrintAt(buffer,width,255,255,255,0,18,"DISK IN");
//	PrintAt(buffer,width,255,255,255,0,21,"CAS ");
//	PrintAt(buffer,width,255,255,255,0,24,"SID 1");
//	PrintAt(buffer,width,255,255,255,0,27,"SID 2");
//	PrintAt(buffer,width,255,255,255,0,30,"SID 3");

	pulsepos=bufferPosition<<16;
	pulsepos+=timeOffset;
	if ((pulsepos>>16)>MAX_CAPTURE)
	{
		pulsepos-=MAX_CAPTURE<<16;
	}
	prevpulsepos=pulsepos;

	DrawTimingA(buffer,width);

	for (a=0;a<MAX_PINS-1;a++)
	{
		for (b=0;b<512*2;b++)
		{
			if (lohi[a][pulsepos>>16])
			{
				buffer[(80+b+(a*8*3+5)*width)*4+0]=0xFF;
				buffer[(80+b+(a*8*3+5)*width)*4+1]=0xFF;
				buffer[(80+b+(a*8*3+5)*width)*4+2]=0xFF;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+0]=0x00;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+1]=0x00;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+2]=0x00;
			}
			else
			{
				buffer[(80+b+(a*8*3+5)*width)*4+0]=0x00;
				buffer[(80+b+(a*8*3+5)*width)*4+1]=0x00;
				buffer[(80+b+(a*8*3+5)*width)*4+2]=0x00;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+0]=0xFF;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+1]=0xFF;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+2]=0xFF;
			}

			if (b!=0 && lohi[a][prevpulsepos>>16]!=lohi[a][pulsepos>>16])
			{
				int c;
				for (c=6;c<8*2+1;c++)
				{
					buffer[(80+b+(a*8*3+c)*width)*4+0]=0xFF;
					buffer[(80+b+(a*8*3+c)*width)*4+1]=0xFF;
					buffer[(80+b+(a*8*3+c)*width)*4+2]=0xFF;
				}
			}
			else
			{
				int c;
				for (c=6;c<8*2+1;c++)
				{
					buffer[(80+b+(a*8*3+c)*width)*4+0]=0x00;
					buffer[(80+b+(a*8*3+c)*width)*4+1]=0x00;
					buffer[(80+b+(a*8*3+c)*width)*4+2]=0x00;
				}
			}
			prevpulsepos=pulsepos;
			pulsepos+=timeStretch;
			if ((pulsepos>>16)>=MAX_CAPTURE)
			{
				pulsepos=0;
			}
		}
	}
}

int KeyDownWindow(int key,GLFWwindow window);

void UpdateTimingWindow(GLFWwindow window)
{
	int a;

	if (CheckKeyWindow(GLFW_KEY_DOWN,window))
	{
		if (timeStretch>1)
			timeStretch>>=1;
		ClearKey(GLFW_KEY_DOWN);
	}
	if (CheckKeyWindow(GLFW_KEY_UP,window))
	{
		if (timeStretch<0x80000000)
			timeStretch<<=1;
		ClearKey(GLFW_KEY_UP);
	}

	if (KeyDownWindow(GLFW_KEY_LEFT,window))
	{
		timeOffset-=8<<16;
	}
	if (KeyDownWindow(GLFW_KEY_RIGHT,window))
	{
		timeOffset+=8<<16;
	}

}


