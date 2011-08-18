#include <GL/glfw3.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "font.h"

unsigned short breakpoints[20]={0x0067};
unsigned int numBreakpoints=0;

int isBreakpoint(unsigned int address)
{
	int a;
	for (a=0;a<numBreakpoints;a++)
	{
		if (address==breakpoints[a])
			return 1;
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

uint16_t PinGetPIN_A();
uint8_t PinGetPIN_D();
uint8_t PinGetPIN_WAIT();
uint8_t PinGetPIN_HLDA();
uint8_t PinGetPIN_SYNC();
uint8_t PinGetPIN__WR();
uint8_t PinGetPIN_DBIN();
uint8_t PinGetPIN_INTE();
uint8_t PinGetPIN_O2();

// Because certain pin's on the cpu are no longer readable external, the following buffers are used so that the timing capture will still work
extern uint8_t PIN_BUFFER_READY;
extern uint8_t PIN_BUFFER_RESET;
extern uint8_t PIN_BUFFER_O1;
extern uint8_t PIN_BUFFER_O2;
extern uint8_t PIN_BUFFER_INT;
extern uint8_t PIN_BUFFER_HOLD;

extern unsigned short SP;
extern unsigned short PC;
extern unsigned short BC;
extern unsigned short DE;
extern unsigned short HL;
extern unsigned char A;
extern unsigned char FLAGS;

#define MAX_CAPTURE		(512*1024)				// 1 Mb of data

unsigned char lohi[36][MAX_CAPTURE];
int bufferPosition=0;

void RecordPins()
{
	lohi[ 0][bufferPosition]=(PinGetPIN_A()&0x8000)>>15;
	lohi[ 1][bufferPosition]=(PinGetPIN_A()&0x4000)>>14;
	lohi[ 2][bufferPosition]=(PinGetPIN_A()&0x2000)>>13;
	lohi[ 3][bufferPosition]=(PinGetPIN_A()&0x1000)>>12;
	lohi[ 4][bufferPosition]=(PinGetPIN_A()&0x0800)>>11;
	lohi[ 5][bufferPosition]=(PinGetPIN_A()&0x0400)>>10;
	lohi[ 6][bufferPosition]=(PinGetPIN_A()&0x0200)>>9;
	lohi[ 7][bufferPosition]=(PinGetPIN_A()&0x0100)>>8;
	lohi[ 8][bufferPosition]=(PinGetPIN_A()&0x0080)>>7;
	lohi[ 9][bufferPosition]=(PinGetPIN_A()&0x0040)>>6;
	lohi[10][bufferPosition]=(PinGetPIN_A()&0x0020)>>5;
	lohi[11][bufferPosition]=(PinGetPIN_A()&0x0010)>>4;
	lohi[12][bufferPosition]=(PinGetPIN_A()&0x0008)>>3;
	lohi[13][bufferPosition]=(PinGetPIN_A()&0x0004)>>2;
	lohi[14][bufferPosition]=(PinGetPIN_A()&0x0002)>>1;
	lohi[15][bufferPosition]=PinGetPIN_A()&0x0001;
	lohi[16][bufferPosition]=(PinGetPIN_D()&0x80)>>7;
	lohi[17][bufferPosition]=(PinGetPIN_D()&0x40)>>6;
	lohi[18][bufferPosition]=(PinGetPIN_D()&0x20)>>5;
	lohi[19][bufferPosition]=(PinGetPIN_D()&0x10)>>4;
	lohi[20][bufferPosition]=(PinGetPIN_D()&0x08)>>3;
	lohi[21][bufferPosition]=(PinGetPIN_D()&0x04)>>2;
	lohi[22][bufferPosition]=(PinGetPIN_D()&0x02)>>1;
	lohi[23][bufferPosition]=PinGetPIN_D()&0x01;
	lohi[24][bufferPosition]=PinGetPIN_WAIT()&0x01;
	lohi[25][bufferPosition]=PIN_BUFFER_READY&0x01;
	lohi[26][bufferPosition]=PIN_BUFFER_O1&0x01;
	lohi[27][bufferPosition]=PinGetPIN_HLDA()&0x01;
	lohi[28][bufferPosition]=PinGetPIN_SYNC()&0x01;
	lohi[29][bufferPosition]=PinGetPIN__WR()&0x01;
	lohi[30][bufferPosition]=PinGetPIN_DBIN()&0x01;
	lohi[31][bufferPosition]=PinGetPIN_INTE()&0x01;
	lohi[32][bufferPosition]=PIN_BUFFER_O2&0x01;
	lohi[33][bufferPosition]=PIN_BUFFER_INT&0x01;
	lohi[34][bufferPosition]=PIN_BUFFER_HOLD&0x01;
	lohi[35][bufferPosition]=PIN_BUFFER_RESET&0x01;

	bufferPosition++;
	if (bufferPosition>=MAX_CAPTURE)
		bufferPosition=0;
}

unsigned int timeStretch=0x10000;

void UpdateTiming(int scrollPos)
{
	timeStretch+=0x0200*scrollPos;
}

const char* decodeDisasm(unsigned int address,int *count);

unsigned char AddressLookup(unsigned int address);

void DrawRegister(unsigned char* buffer,unsigned int width)
{
	unsigned int address=PC-1,b;
	int numBytes=3;

	PrintAt(buffer,width,255,255,255,0,0,"FLAGS = S  Z  0  AC 0  P  1  CY");

	PrintAt(buffer,width,255,255,255,0,1,"        %s  %s  %s  %s  %s  %s  %s  %s\n",
			FLAGS&0x80 ? "1" : "0",
			FLAGS&0x40 ? "1" : "0",
			FLAGS&0x20 ? "1" : "0",
			FLAGS&0x10 ? "1" : "0",
			FLAGS&0x08 ? "1" : "0",
			FLAGS&0x04 ? "1" : "0",
			FLAGS&0x02 ? "1" : "0",
			FLAGS&0x01 ? "1" : "0");

	PrintAt(buffer,width,255,255,255,0,2,"A   %02X\n",A);
	PrintAt(buffer,width,255,255,255,0,3,"BC  %04X\n",BC);
	PrintAt(buffer,width,255,255,255,0,4,"DE  %04X\n",DE);
	PrintAt(buffer,width,255,255,255,0,5,"HL  %04X\n",HL);
	PrintAt(buffer,width,255,255,255,0,6,"SP  %04X\n",SP);
	PrintAt(buffer,width,255,255,255,0,7,"PC  %04X\n",PC);

	for (b=0;b<16;b++)
	{
		int a;
		const char* retVal = decodeDisasm(address,&numBytes);
		int opcodeLength=numBytes+1;
		unsigned char colR=255,colG=255,colB=255;

		for (a=0;a<numBreakpoints;a++)
		{
			if (address==breakpoints[a])
			{
				colG=0;
				colB=0;
			}
		}

		PrintAt(buffer,width,colR,colG,colB,0,10+b,"%04X  ",address);

		PrintAt(buffer,width,255,255,255,8,10+b,"            ");
		for (a=0;a<opcodeLength;a++)
		{
			PrintAt(buffer,width,colR,colG,colB,8+a*3,10+b,"%02X ",AddressLookup(address+a));
		}
		PrintAt(buffer,width,255,255,255,8+4*3,10+b,"            ");
		PrintAt(buffer,width,colR,colG,colB,8+4*3,10+b,"%s",retVal);

		address+=opcodeLength;
	}
}

void DrawTiming(unsigned char* buffer,unsigned int width)
{
	int a,b;
	unsigned int pulsepos;
	unsigned int prevpulsepos;

	PrintAt(buffer,width,255,255,255,0,0,"PIN_A");
	PrintAt(buffer,width,255,255,255,0,16,"PIN_D");
	PrintAt(buffer,width,255,255,255,0,24,"PIN_WAIT");
	PrintAt(buffer,width,255,255,255,0,25,"PIN_READY");
	PrintAt(buffer,width,255,255,255,0,26,"PIN_O1");
	PrintAt(buffer,width,255,255,255,0,27,"PIN_HLDA");
	PrintAt(buffer,width,255,255,255,0,28,"PIN_SYNC");
	PrintAt(buffer,width,255,255,255,0,29,"PIN__WR");
	PrintAt(buffer,width,255,255,255,0,30,"PIN_DBIN");
	PrintAt(buffer,width,255,255,255,0,31,"PIN_INTE");
	PrintAt(buffer,width,255,255,255,0,32,"PIN_O2");
	PrintAt(buffer,width,255,255,255,0,33,"PIN_INT");
	PrintAt(buffer,width,255,255,255,0,34,"PIN_HOLD");
	PrintAt(buffer,width,255,255,255,0,35,"PIN_RESET");

	pulsepos=bufferPosition<<16;
	prevpulsepos=bufferPosition<<16;

	for (a=0;a<36;a++)
	{
		for (b=0;b<600;b++)
		{
			if (lohi[a][pulsepos>>16])
			{
				buffer[(80+b+(a*8+1)*width)*4+0]=0xFF;
				buffer[(80+b+(a*8+1)*width)*4+1]=0xFF;
				buffer[(80+b+(a*8+1)*width)*4+2]=0xFF;
				buffer[(80+b+(a*8+6)*width)*4+0]=0x00;
				buffer[(80+b+(a*8+6)*width)*4+1]=0x00;
				buffer[(80+b+(a*8+6)*width)*4+2]=0x00;
			}
			else
			{
				buffer[(80+b+(a*8+1)*width)*4+0]=0x00;
				buffer[(80+b+(a*8+1)*width)*4+1]=0x00;
				buffer[(80+b+(a*8+1)*width)*4+2]=0x00;
				buffer[(80+b+(a*8+6)*width)*4+0]=0xFF;
				buffer[(80+b+(a*8+6)*width)*4+1]=0xFF;
				buffer[(80+b+(a*8+6)*width)*4+2]=0xFF;
			}

			if (lohi[a][prevpulsepos>>16]!=lohi[a][pulsepos>>16])
			{
				int c;
				for (c=2;c<6;c++)
				{
					buffer[(80+b+(a*8+c)*width)*4+0]=0xFF;
					buffer[(80+b+(a*8+c)*width)*4+1]=0xFF;
					buffer[(80+b+(a*8+c)*width)*4+2]=0xFF;
				}
			}
			else
			{
				int c;
				for (c=2;c<6;c++)
				{
					buffer[(80+b+(a*8+c)*width)*4+0]=0x00;
					buffer[(80+b+(a*8+c)*width)*4+1]=0x00;
					buffer[(80+b+(a*8+c)*width)*4+2]=0x00;
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

