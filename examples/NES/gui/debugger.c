#include <GL/glfw3.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

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

extern uint8_t palIndex[0x20];

uint32_t nesColours[0x40];
extern uint16_t	 PPU_FV;		// 3 bits
extern uint16_t	 PPU_FVc;
extern uint16_t	 PPU_V;			// 1 bit
extern uint16_t	 PPU_Vc;
extern uint16_t	 PPU_H;			// 1 bit
extern uint16_t	 PPU_Hc;
extern uint16_t	 PPU_VT;		// 5 bits
extern uint16_t	 PPU_VTc;
extern uint16_t	 PPU_HT;		// 5 bits
extern uint16_t	 PPU_HTc;
extern uint16_t  PPU_FH;		// 3 bits
extern uint16_t	 PPU_S;			// 1 bits
extern uint16_t	 PPU_PAR;		// 8 bits
extern uint16_t	 PPU_AR;		// 2 bits

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
/*
	if (chip==0)
	{
		uint16_t nameTableAddressC=0x2000;
		uint16_t nameTableAddressL=0x2000;

		nameTableAddressC|=PPU_Vc<<11;
		nameTableAddressC|=PPU_Hc<<10;
		nameTableAddressC|=PPU_VTc<<5;
		nameTableAddressC|=PPU_HTc;
		nameTableAddressL|=PPU_V<<11;
		nameTableAddressL|=PPU_H<<10;
		nameTableAddressL|=PPU_VT<<5;
		nameTableAddressL|=PPU_HT;
		
		PrintAt(buffer,width,255,255,255,0,25,"%04X %04X %02X",nameTableAddressC,nameTableAddressL,PPU_HTc);
	}
*/
	for (a=0;a<32;a++)
	{
		uint32_t colour;
		colour = nesColours[palIndex[a]];
		if ((a&0x3)==0)
			colour = nesColours[palIndex[0]];
		PrintAt(buffer,width,(colour&0x00FF0000)>>16,(colour&0x0000FF00)>>8,(colour&0x000000FF),a&0xf,27+(a/16),"#");
	}

	extern uint16_t PPU_hClock;
	extern uint16_t PPU_vClock;

	PrintAt(buffer,width,255,255,255,0,31,"vClock %04X : hClock %04X",PPU_vClock&0x1FF,PPU_hClock&0x1FF);
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

