/*
 * ZXSpectrum
 *
 *  Uses Z80 step core built in EDL
 *  Rest is C for now
 */

#include <GL/glfw3.h>
#include <GL/glext.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void STEP(void);
void RESET(void);
void INTERRUPT(uint8_t);

unsigned char Rom[0x4000];
unsigned char Ram[0xC000];

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
	if (LoadRom(0x0000,0x4000,"roms/48k.rom"))
		return 1;

	return 0;
}

int masterClock=0;
int pixelClock=0;

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

unsigned char NEXTINT;

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	256

#define TIMING_WIDTH	680
#define TIMING_HEIGHT	400

#define BORDERWIDTH	(16)

#define WIDTH	(256+(2*BORDERWIDTH))
#define	HEIGHT	(192+(2*BORDERWIDTH))

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0

GLFWwindow windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

uint8_t flashTimer=0;
uint8_t borderCol=0;

uint32_t GetColour(uint8_t attr,uint8_t pixSet)
{
	uint32_t colour=0;
	uint8_t ink = attr&0x07;
	uint8_t paper = (attr&0x38)>>3;
	uint8_t bright = (attr&0x40)>>6;
	uint8_t flash = (attr&0x80)>>7;

	if (flash && flashTimer&0x10)
	{
		uint8_t swap = paper;
		paper=ink;
		ink=swap;
	}

	if (pixSet)
	{
		if (ink&0x01)
		{
			if (bright)
				colour|=0x000000FF;
			else
				colour|=0x000000CC;
		}
		if (ink&0x02)
		{
			if (bright)
				colour|=0x00FF0000;
			else
				colour|=0x00CC0000;
		}
		if (ink&0x04)
		{
			if (bright)
				colour|=0x0000FF00;
			else
				colour|=0x0000CC00;
		}
	}
	else
	{
		if (paper&0x01)
		{
			if (bright)
				colour|=0x000000FF;
			else
				colour|=0x000000CC;
		}
		if (paper&0x02)
		{
			if (bright)
				colour|=0x00FF0000;
			else
				colour|=0x00CC0000;
		}
		if (paper&0x04)
		{
			if (bright)
				colour|=0x0000FF00;
			else
				colour|=0x0000CC00;
		}
	}

	return colour;
}

void ClearBorderColour()
{
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	int x,y;

	uint32_t BGRA;

	BGRA=GetColour(borderCol,1);

	for (y=0;y<HEIGHT;y++)
	{
		for (x=0;x<WIDTH;x++)
		{
			outputTexture[x+y*WIDTH]=BGRA;
		}
	}
}

uint8_t *GetScreenPtr()
{
	return Ram;
}

uint8_t *GetAttrPtr()
{
	return GetScreenPtr()+24*32*8;
}

void VID_DrawScreen()
{
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	uint8_t *screenPtr = GetScreenPtr();
	uint8_t *attrPtr = GetAttrPtr();
	int x,sx,y;

	ClearBorderColour();
	for (y=0;y<HEIGHT-2*BORDERWIDTH;y++)
	{
		uint8_t *rowPtr = &screenPtr[(y/64)*(8*8*32) + (y&7)*32*8 + ((y&63)>>3)*32];

		for (x=0;x<(WIDTH-2*BORDERWIDTH)/8;x++)
		{
			uint8_t scrPixel = rowPtr[x];
			uint8_t xMask=0x80;
			uint8_t attr = attrPtr[x+(y/8)*32];
			uint32_t BGRA;

			for (sx=0;sx<8;sx++)
			{
				BGRA = GetColour(attr,scrPixel&xMask);

				outputTexture[x*8+sx+BORDERWIDTH+(y+BORDERWIDTH)*WIDTH]=BGRA;

				xMask>>=1;
			}
		}
	}
	flashTimer++;
}

void ShowScreen(int windowNum,int w,int h)
{
	VID_DrawScreen();

	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f,1.0f);

	glTexCoord2f(0.0f, h);
	glVertex2f(-1.0f,-1.0f);

	glTexCoord2f(w, h);
	glVertex2f(1.0f,-1.0f);

	glTexCoord2f(w, 0.0f);
	glVertex2f(1.0f,1.0f);
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

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);

	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA, w,
			h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);

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

extern unsigned short PC;
extern unsigned char CYCLES;

#define TSTATES_PER_ROW		224
#define	ROWS_PER_VBLANK		312

extern uint8_t *DIS_[256];

uint8_t GetByte(uint16_t addr)
{
	if (addr>=0x4000)
		return Ram[addr-0x4000];
	
	return Rom[addr];
}

void SetByte(uint16_t addr,uint8_t byte)
{
	if (addr>=0x4000)
	{
		Ram[addr-0x4000]=byte;
	}
}

uint8_t CheckKeys(uint8_t scan,int key0,int key1,int key2,int key3,int key4)
{
	uint8_t retVal=0;
	if (scan)
	{
		if (KeyDown(key0))
		{
			retVal|=0x01;
		}
		if (KeyDown(key1))
		{
			retVal|=0x02;
		}
		if (KeyDown(key2))
		{
			retVal|=0x04;
		}
		if (KeyDown(key3))
		{
			retVal|=0x08;
		}
		if (KeyDown(key4))
		{
			retVal|=0x10;
		}
	}

	return retVal;
}

uint8_t GetPort(uint16_t port)
{
	uint8_t matrixLine = ~(port>>8);
	uint8_t retVal=0x0;

	if ((port&1)==0)			// ula responds on all even addresses
	{
		retVal|=CheckKeys(matrixLine&0x80,GLFW_KEY_SPACE,GLFW_KEY_RCTRL,'M','N','B');
		retVal|=CheckKeys(matrixLine&0x40,GLFW_KEY_ENTER,'L','K','J','H');
		retVal|=CheckKeys(matrixLine&0x20,'P','O','I','U','Y');
		retVal|=CheckKeys(matrixLine&0x10,'0','9','8','7','6');
		retVal|=CheckKeys(matrixLine&0x08,'1','2','3','4','5');
		retVal|=CheckKeys(matrixLine&0x04,'Q','W','E','R','T');
		retVal|=CheckKeys(matrixLine&0x02,'A','S','D','F','G');
		retVal|=CheckKeys(matrixLine&0x01,GLFW_KEY_LSHIFT,'Z','X','C','V');

		retVal=(~retVal)&0x1F;

		retVal|=0xA0;			// apparantly always logic 1

		return retVal;
	}

	return 0xFF;
}

void SetPort(uint16_t port,uint8_t byte)
{
	if ((port&0x01)==0)				// ula responds on all even addresses
	{
		borderCol=byte&0x07;
	}
}

extern uint8_t *DIS_[256];
extern uint8_t *DIS_CB[256];
extern uint8_t *DIS_DD[256];
extern uint8_t *DIS_DDCB[256];
extern uint8_t *DIS_ED[256];
extern uint8_t *DIS_FD[256];
extern uint8_t *DIS_FDCB[256];

extern uint16_t	AF;
extern uint16_t	BC;
extern uint16_t	DE;
extern uint16_t	HL;
extern uint16_t	IX;
extern uint16_t	IY;
extern uint16_t	PC;
extern uint16_t	SP;

void DUMP_REGISTERS()
{
	printf("--------\n");
	printf("FLAGS = S  Z  -  H  -  P  N  C\n");
	printf("        %s  %s  %s  %s  %s  %s  %s  %s\n",
			AF&0x80 ? "1" : "0",
			AF&0x40 ? "1" : "0",
			AF&0x20 ? "1" : "0",
			AF&0x10 ? "1" : "0",
			AF&0x08 ? "1" : "0",
			AF&0x04 ? "1" : "0",
			AF&0x02 ? "1" : "0",
			AF&0x01 ? "1" : "0");
	printf("AF= %04X\n",AF);
	printf("BC= %04X\n",BC);
	printf("DE= %04X\n",DE);
	printf("HL= %04X\n",HL);
	printf("IX= %04X\n",IX);
	printf("IY= %04X\n",IY);
	printf("SP= %04X\n",SP);
	printf("--------\n");
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

       	const char* mnemonic=table[GetByte(address)];
	const char* sPtr=mnemonic;
	char* dPtr=temporaryBuffer;
	int counting = 0;
	int doingDecode=0;

	if (strcmp(mnemonic,"CB")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_CB,address+1,&tmpCount);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"DD")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_DD,address+1,&tmpCount);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"DDCB")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_DDCB,address+2,&tmpCount);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"FDCB")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_FDCB,address+2,&tmpCount);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"ED")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_ED,address+1,&tmpCount);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"FD")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_FD,address+1,&tmpCount);
		*count=tmpCount+1;
		return temporaryBuffer;
	}

	while (*sPtr)
	{
		if (!doingDecode)
		{
			if (*sPtr=='%')
			{
				doingDecode=1;
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
	return temporaryBuffer;
}

void Disassemble(unsigned int address)
{
	int a;
	int numBytes=0;
	const char* retVal = decodeDisasm(DIS_,address,&numBytes);

	DUMP_REGISTERS();
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
}

int main(int argc,char**argv)
{
	double	atStart,now,remain;

	/// Initialize GLFW 
	glfwInit(); 

	// Open invaders OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwOpenWindow( WIDTH, HEIGHT, GLFW_WINDOWED,"spectrum",NULL)) ) 
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
	
	RESET();

	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
	{
//		Disassemble(PC);
		STEP();

		masterClock+=CYCLES;
		while (masterClock>=TSTATES_PER_ROW)
		{	
			pixelClock+=1;
			masterClock-=TSTATES_PER_ROW;
		}
		if (pixelClock>=ROWS_PER_VBLANK)
		{
			pixelClock-=ROWS_PER_VBLANK;

			INTERRUPT(0xFF);

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
				
			glfwPollEvents();
			
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

