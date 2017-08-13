#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "font.h"

void ClearKey(int key);
int CheckKeyWindow(int key,GLFWwindow* window);

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

void DrawCharPixel(unsigned char* buffer,unsigned int width,unsigned int x,unsigned int y, char c,unsigned char r,unsigned char g,unsigned char b)
{
	unsigned int xx,yy;
	unsigned char *fontChar=&FontData[c*6*8];
	
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

void PrintAtPixel(unsigned char* buffer,unsigned int width,unsigned char r,unsigned char g,unsigned char b,unsigned int x,unsigned int y,const char *msg,...)
{
	static char tStringBuffer[32768];
	char *pMsg=tStringBuffer;
	va_list args;

	va_start (args, msg);
	vsprintf (tStringBuffer,msg, args);
	va_end (args);

	while (*pMsg)
	{
		DrawCharPixel(buffer,width,x,y,*pMsg,r,g,b);
		x+=8;
		pMsg++;
	}
}

#define MAX_CAPTURE		(1024*1024)
#define MAX_PINS		(36)

unsigned char lohi[MAX_PINS][MAX_CAPTURE];

int bufferPosition=0;
int bufferScroll = 0;
uint32_t timeStretch=0x100;
int pinNum = 0;
void RecordPin(int pinPos,uint8_t pinVal)
{
	lohi[pinPos][bufferPosition]=pinVal&1;
}

void UpdatePinTick()
{
	bufferPosition++;
	if (bufferPosition>=MAX_CAPTURE)
		bufferPosition=0;
}

#define TOT_PINS 6+16+8+2+2+2
//#define TOT_PINS 26

#define NUM_PIN_DATA	2
uint8_t pinData[2] = { TOT_PINS - 8 - 2, TOT_PINS - 8 - 2 - 4 - 16 };
uint8_t pinWidth[2] = { 8, 16 };

void BusTap(uint8_t bits, uint32_t val)
{
	static int done14 = 0;
	static int done8 = 0;
	if (done14==1 && bits == 14)
	{
		done14 = 0;
		return;
	}
	if (done8==1 && bits == 8)
	{
		done8 = 0;
		return;
	}
	if (bits == 14)
		done14 = 1;
	if (bits == 8)
		done8 = 1;
	for (int a = 0; a < bits; a++)
	{
		RecordPin(pinNum, (val>>((bits-1)-a)) & 1);
		pinNum++;
		if (pinNum == TOT_PINS)
		{
			UpdatePinTick();
			pinNum = 0;
		}
	}
}

void DrawTiming(unsigned char* buffer,unsigned int width,unsigned int height)
{
	int a,b;
	unsigned int pulsepos;
	unsigned int prevpulsepos;

	memset(buffer, 0, width*height * 4);

	PrintAt(buffer,width,255,255,255,0,0,"MCLK");
	PrintAt(buffer,width,255,255,255,0,3,"CCLK");
	PrintAt(buffer,width,255,255,255,0,6,"~IO");
	PrintAt(buffer,width,255,255,255,0,9,"~MRQ");
	PrintAt(buffer,width,255,255,255,0,12,"~RD");
	PrintAt(buffer,width,255,255,255,0,15,"~WR");
	PrintAt(buffer,width,255,255,255,0,18,"A");
	PrintAt(buffer,width,255,255,255,0,18+16*3,"~INT");
	PrintAt(buffer,width,255,255,255,0,18+17*3,"~LRMCS");
	PrintAt(buffer,width,255,255,255,0,18+18*3,"~HRMCS");
	PrintAt(buffer,width,255,255,255,0,18+19*3,"~ROMCS");
	PrintAt(buffer,width,255,255,255,0,18+20*3,"D");
	PrintAt(buffer, width, 255, 255, 255, 0, 18 + 28 * 3, "M1");
	PrintAt(buffer, width, 255, 255, 255, 0, 18 + 29 * 3, "RFSH");
	//	PrintAt(buffer,width,255,255,255,0,24,"SID 1");
//	PrintAt(buffer,width,255,255,255,0,27,"SID 2");
//	PrintAt(buffer,width,255,255,255,0,30,"SID 3");

	bufferPosition = 0;
	for (a=0;a<MAX_PINS;a++)
	{
		pulsepos = bufferPosition << 8;
		prevpulsepos = bufferPosition << 8;

		pulsepos += bufferScroll << 8;
		while (pulsepos < 0)
			pulsepos += MAX_CAPTURE<<8;
		while ((pulsepos>>8) >= MAX_CAPTURE)
			pulsepos -= MAX_CAPTURE<<8;
		prevpulsepos = pulsepos;
		for (b=0;b<640*2 - 90;b++)
		{
			if (a<2 && b != 0 && lohi[a][prevpulsepos >> 8] != lohi[a][pulsepos >> 8])
			{
				int c;
				for (c=6;c<height - a*8*3;c++)
				{
					buffer[(80+b+(a*8*3+c)*width)*4+0]=64 + a*48;
					buffer[(80+b+(a*8*3+c)*width)*4+1]=64 + a*48;
					buffer[(80+b+(a*8*3+c)*width)*4+2]=64 + a*48;
				}
			}
			if (lohi[a][pulsepos>>8])
			{
				buffer[(80+b+(a*8*3+5)*width)*4+0]=0xFF;
				buffer[(80+b+(a*8*3+5)*width)*4+1]=0xFF;
				buffer[(80+b+(a*8*3+5)*width)*4+2]=0xFF;
				/*buffer[(80+b+(a*8*3+8*2+1)*width)*4+0]=0x00;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+1]=0x00;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+2]=0x00;*/
			}
			else
			{
				/*buffer[(80+b+(a*8*3+5)*width)*4+0]=0x00;
				buffer[(80+b+(a*8*3+5)*width)*4+1]=0x00;
				buffer[(80+b+(a*8*3+5)*width)*4+2]=0x00;*/
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+0]=0xFF;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+1]=0xFF;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+2]=0xFF;
			}

			if (b!=0 && lohi[a][prevpulsepos>>8]!=lohi[a][pulsepos>>8])
			{
				int c;
				for (c=6;c<8*2+1;c++)
				{
					buffer[(80+b+(a*8*3+c)*width)*4+0]=0xFF;
					buffer[(80+b+(a*8*3+c)*width)*4+1]=0xFF;
					buffer[(80+b+(a*8*3+c)*width)*4+2]=0xFF;
				}
			}
			/*else
			{
				int c;
				for (c=6;c<8*2+1;c++)
				{
					buffer[(80+b+(a*8*3+c)*width)*4+0]=0x00;
					buffer[(80+b+(a*8*3+c)*width)*4+1]=0x00;
					buffer[(80+b+(a*8*3+c)*width)*4+2]=0x00;
				}
			}*/

			prevpulsepos=pulsepos;
			pulsepos+=timeStretch;
			if ((pulsepos>>8)>=MAX_CAPTURE)
			{
				pulsepos=0;
			}
		}
	}

	// Test decode D
	bufferPosition = 0;
	for (int a=0;a<NUM_PIN_DATA;a++)
	{
		uint32_t lastDataValue = 0xFFFFFFFF;
		uint8_t dataPos = pinData[a];

		pulsepos = bufferPosition << 8;
		prevpulsepos = bufferPosition << 8;

		pulsepos += bufferScroll << 8;
		while (pulsepos < 0)
			pulsepos += MAX_CAPTURE<<8;
		while ((pulsepos>>8) >= MAX_CAPTURE)
			pulsepos -= MAX_CAPTURE<<8;
		prevpulsepos = pulsepos;

		for (b=0;b<640*2 - 90;b++)
		{
			uint16_t dataValue = 0;

			for (int r = 0; r < pinWidth[a]; r++)
			{
				dataValue <<= 1;
				dataValue |= lohi[dataPos + r][pulsepos >> 8] ? 1 : 0;
			}

			if (dataValue != lastDataValue)
			{
				if (pinWidth[a]==8)
					PrintAtPixel(buffer,width,255,255,255,84+b,dataPos*8*3+7,"%02X",(uint8_t)dataValue);
				else
					PrintAtPixel(buffer,width,255,255,255,84+b,dataPos*8*3+7,"%04X",dataValue);
				lastDataValue = dataValue;
			}

			prevpulsepos=pulsepos;
			pulsepos+=timeStretch;
			if ((pulsepos>>8)>=MAX_CAPTURE)
			{
				pulsepos=0;
			}
		}
	}
	bufferPosition = 0;
}

void UpdateTimingWindow(GLFWwindow* window)
{
	if (CheckKey(GLFW_KEY_DOWN))
	{
		if (timeStretch>1)
			timeStretch>>=1;
		ClearKey(GLFW_KEY_DOWN);
	}
	if (CheckKey(GLFW_KEY_UP))
	{
		if (timeStretch<0x80000000)
			timeStretch<<=1;
		ClearKey(GLFW_KEY_UP);
	}
	if (KeyDown(GLFW_KEY_LEFT))
	{
		if (KeyDown(GLFW_KEY_RIGHT_SHIFT))
			bufferScroll -= 16;
		else
			bufferScroll--;
	}
	if (KeyDown(GLFW_KEY_RIGHT))
	{
		if (KeyDown(GLFW_KEY_RIGHT_SHIFT))
			bufferScroll += 16;
		else
			bufferScroll++;
	}
	if (CheckKey(GLFW_KEY_END))
	{
		ClearKey(GLFW_KEY_END);
		for (int a = 0; a < MAX_CAPTURE; a++)
		{
			if (lohi[22][a] == 0)
			{
				bufferScroll = a;
				break;
			}
		}
	}
}

