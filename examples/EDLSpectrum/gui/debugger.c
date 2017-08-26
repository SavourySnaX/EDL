#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "font.h"

void ClearKey(int key);
int KeyDown(int key);
int CheckKey(int key);

unsigned short breakpoints[2][20]={
	{0x44E,0},
	{/*0xE853,*//*0xF556,*/0xfd27,0xfd86,0xfd58,0xF497,0xF3C0,0xF567}
	};		// first list main cpu, second is disk cpu
unsigned int numBreakpoints[2]={0,0};

unsigned short curAddresses[2][16];
int Offs[2]={0,0};

void ToggleBreakPoint(int chip)
{
	size_t a,c;

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
	size_t a;
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
	vsprintf_s(tStringBuffer,sizeof(tStringBuffer),msg, args);
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
	vsprintf_s(tStringBuffer,sizeof(tStringBuffer),msg, args);
	va_end (args);

	while (*pMsg)
	{
		DrawCharPixel(buffer,width,x,y,*pMsg,r,g,b);
		x+=8;
		pMsg++;
	}
}

#define MAX_CAPTURE		(1024*1024)
#define MAX_PINS		(64)

unsigned char lohi[MAX_PINS][MAX_CAPTURE];
const char* pinNames[MAX_PINS];

int bufferPosition=0;
int bufferScroll = 0;
uint32_t timeStretch=0x100;
int pinNum = 0;
void RecordPin(int pinPos,uint8_t pinVal)
{
	lohi[pinPos][bufferPosition]=pinVal&1;
}

uint32_t pinsToDisplay = 0;

void UpdatePinTick()
{
	bufferPosition++;
	if (bufferPosition>=MAX_CAPTURE)
		bufferPosition=0;
}

#define TOT_PINS 6+16+8+2+2+2
//#define TOT_PINS 26

uint32_t numPinData = 0;
uint8_t pinsSet = 0;

#define MAX_PIN_DATA	32
uint8_t pinData[MAX_PIN_DATA];
uint8_t pinWidth[MAX_PIN_DATA];

void BusTap(uint8_t bits, uint32_t val,const char* name,uint32_t decodeWidth, uint8_t last)
{
	if (!pinsSet)
	{
		pinNames[pinNum] = name;
		if (decodeWidth)
		{
			pinData[numPinData] = pinNum;
			pinWidth[numPinData] = decodeWidth;
			numPinData++;
		}
	}
	for (int a = 0; a < bits; a++)
	{
		RecordPin(pinNum, (val>>((bits-1)-a)) & 1);
		pinNum++;
	}
	if (last)
	{
		pinsToDisplay = pinNum;
		UpdatePinTick();
		pinNum = 0;
		pinsSet = 1;
	}
}

void DrawTiming(unsigned char* buffer,unsigned int width,unsigned int height)
{
	uint32_t a;
	int b;
	unsigned int pulsepos;
	unsigned int prevpulsepos;

	memset(buffer, 0, width*height * 4);

	bufferPosition = 0;
	for (a=0;a<pinsToDisplay;a++)
	{
		if (pinNames[a] != NULL)
		{
			PrintAt(buffer, width, 255, 255, 255, 0, a * 3, pinNames[a]);
		}
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
			// TODO make the vertical lines optional (part of the extended bus tap args?)
			if (a<2 && b != 0 && lohi[a][prevpulsepos >> 8] != lohi[a][pulsepos >> 8])
			{
				unsigned int c;
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
			}
			else
			{
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

			prevpulsepos=pulsepos;
			pulsepos+=timeStretch;
			if ((pulsepos>>8)>=MAX_CAPTURE)
			{
				pulsepos=0;
			}
		}
	}

	// Decode data routine (for display hex values of pin groups)
	bufferPosition = 0;
	for (uint32_t a=0;a<numPinData;a++)
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

