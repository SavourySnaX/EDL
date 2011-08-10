#include <GL/glfw3.h>

#include <stdlib.h>
#include <stdarg.h>

#include "font.h"

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
			buffer[(x+xx+1 + (y+yy)*width)*4+0]=(*fontChar)*r;
			buffer[(x+xx+1 + (y+yy)*width)*4+1]=(*fontChar)*g;
			buffer[(x+xx+1 + (y+yy)*width)*4+2]=(*fontChar)*b;
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

extern unsigned short PIN_A;
extern unsigned char PIN_D;
extern unsigned char PIN_WAIT;
extern unsigned char PIN_READY;
extern unsigned char PIN_O1;
extern unsigned char PIN_HLDA;
extern unsigned char PIN_SYNC;
extern unsigned char PIN__WR;
extern unsigned char PIN_DBIN;
extern unsigned char PIN_INTE;
extern unsigned char PIN_O2;
extern unsigned char PIN_INT;
extern unsigned char PIN_HOLD;
extern unsigned char PIN_RESET;

#define MAX_CAPTURE		(600)

unsigned char lohi[36][MAX_CAPTURE];
int bufferPosition=0;
void RecordPins()
{
	lohi[ 0][bufferPosition]=(PIN_A&0x8000)>>15;
	lohi[ 1][bufferPosition]=(PIN_A&0x4000)>>14;
	lohi[ 2][bufferPosition]=(PIN_A&0x2000)>>13;
	lohi[ 3][bufferPosition]=(PIN_A&0x1000)>>12;
	lohi[ 4][bufferPosition]=(PIN_A&0x0800)>>11;
	lohi[ 5][bufferPosition]=(PIN_A&0x0400)>>10;
	lohi[ 6][bufferPosition]=(PIN_A&0x0200)>>9;
	lohi[ 7][bufferPosition]=(PIN_A&0x0100)>>8;
	lohi[ 8][bufferPosition]=(PIN_A&0x0080)>>7;
	lohi[ 9][bufferPosition]=(PIN_A&0x0040)>>6;
	lohi[10][bufferPosition]=(PIN_A&0x0020)>>5;
	lohi[11][bufferPosition]=(PIN_A&0x0010)>>4;
	lohi[12][bufferPosition]=(PIN_A&0x0008)>>3;
	lohi[13][bufferPosition]=(PIN_A&0x0004)>>2;
	lohi[14][bufferPosition]=(PIN_A&0x0002)>>1;
	lohi[15][bufferPosition]=PIN_A&0x0001;
	lohi[16][bufferPosition]=(PIN_D&0x80)>>7;
	lohi[17][bufferPosition]=(PIN_D&0x40)>>6;
	lohi[18][bufferPosition]=(PIN_D&0x20)>>5;
	lohi[19][bufferPosition]=(PIN_D&0x10)>>4;
	lohi[20][bufferPosition]=(PIN_D&0x08)>>3;
	lohi[21][bufferPosition]=(PIN_D&0x04)>>2;
	lohi[22][bufferPosition]=(PIN_D&0x02)>>1;
	lohi[23][bufferPosition]=PIN_D&0x01;
	lohi[24][bufferPosition]=PIN_WAIT&0x01;
	lohi[25][bufferPosition]=PIN_READY&0x01;
	lohi[26][bufferPosition]=PIN_O1&0x01;
	lohi[27][bufferPosition]=PIN_HLDA&0x01;
	lohi[28][bufferPosition]=PIN_SYNC&0x01;
	lohi[29][bufferPosition]=PIN__WR&0x01;
	lohi[30][bufferPosition]=PIN_DBIN&0x01;
	lohi[31][bufferPosition]=PIN_INTE&0x01;
	lohi[32][bufferPosition]=PIN_O2&0x01;
	lohi[33][bufferPosition]=PIN_INT&0x01;
	lohi[34][bufferPosition]=PIN_HOLD&0x01;
	lohi[35][bufferPosition]=PIN_RESET&0x01;

	bufferPosition++;
	if (bufferPosition>=MAX_CAPTURE)
		bufferPosition=0;
}

void DrawTiming(unsigned char* buffer,unsigned int width)
{
	int a,b;

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


	for (a=0;a<36;a++)
	{
		for (b=0;b<600;b++)
		{
			if (lohi[a][b])
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

			if (b>0 && lohi[a][b-1]!=lohi[a][b])
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
		}
	}
}

