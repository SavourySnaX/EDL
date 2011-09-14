/*
 * ZXSpectrum
 *
 * Common Functionality Between Pin Accurate Core and Simple Core
 *
 */

#include <GL/glfw3.h>
#include <GL/glext.h>

#include <al.h>
#include <alc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

unsigned char Rom[0x4000];		// Rom Chip
unsigned char Ram[0x4000];		// ULA Connected Ram Chip
unsigned char RamExtra[0x8000];		// 32K Expansion Ram Chip

void AudioKill();
void AudioInitialise();
void UpdateAudio(int numClocks);
void _AudioAddData(int channel,int16_t dacValue);

#define TSTATES_PER_ROW		224
#define	ROWS_PER_VBLANK		312

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

	if (flash && (flashTimer&0x10))
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

void ClearBorderColour(int y)
{
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	int x;

	uint32_t BGRA;

	if (y==ROWS_PER_VBLANK-1)
		flashTimer++;
	if (y>=HEIGHT)
		return;

	BGRA=GetColour(borderCol,1);

	for (x=0;x<WIDTH;x++)
	{
		outputTexture[x+y*WIDTH]=BGRA;
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

void VID_DrawScreenRow(int y)
{
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	uint8_t *screenPtr = GetScreenPtr();
	uint8_t *attrPtr = GetAttrPtr();
	int x,sx;

	ClearBorderColour(y);

	if (y<BORDERWIDTH || y>=HEIGHT-BORDERWIDTH)
		return;

	y-=BORDERWIDTH;
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
}

void ShowScreen(int windowNum,int w,int h)
{
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

extern uint8_t *DIS_[256];

uint8_t GetByte(uint16_t addr)
{
	uint16_t chipSelect = addr&0xC000;

	switch (chipSelect)
	{
		case 0x0000:
			return Rom[addr];
		case 0x4000:
			return Ram[addr&0x3FFF];
		case 0x8000:
		case 0xC000:
			return RamExtra[addr&0x7FFF];
	}
}

void SetByte(uint16_t addr,uint8_t byte)
{
	uint16_t chipSelect = addr&0xC000;

	switch (chipSelect)
	{
		case 0x0000:
			return;
		case 0x4000:
			Ram[addr&0x3FFF]=byte;
			break;
		case 0x8000:
		case 0xC000:
			RamExtra[addr&0x7FFF]=byte;
			break;
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

int tapePaused=1;
int syncOff=0;

void LoadTapeFormat(const char*);
void UpdateTape(uint8_t cycles);
uint8_t GetTapeLevel();
void LoadSNA();

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
		if (GetTapeLevel()>=0x80)
		{
			retVal|=0x40;
		}

		return retVal;
	}

	return 0xFF;
}

void SetPort(uint16_t port,uint8_t byte)
{
	if ((port&0x01)==0)				// ula responds on all even addresses
	{
		borderCol=byte&0x07;
		if (byte&0x10)
		{
			_AudioAddData(0,32767);
		}
		else
		{
			_AudioAddData(0,-32768);
		}
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
extern uint16_t	_AF;
extern uint16_t	_BC;
extern uint16_t	_DE;
extern uint16_t	_HL;
extern uint16_t	IX;
extern uint16_t	IY;
extern uint16_t	PC;
extern uint16_t	SP;
extern uint16_t	IR;

extern uint8_t IM;
extern uint8_t IFF1;
extern uint8_t IFF2;


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
	printf("AF'= %04X\n",_AF);
	printf("BC'= %04X\n",_BC);
	printf("DE'= %04X\n",_DE);
	printf("HL'= %04X\n",_HL);
	printf("IX= %04X\n",IX);
	printf("IY= %04X\n",IY);
	printf("IR= %04X\n",IR);
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
	
	if (sPtr==NULL)
	{
		sprintf(temporaryBuffer,"UNKNOWN OPCODE");
		return temporaryBuffer;
	}

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

int Disassemble(unsigned int address,int registers)
{
	int a;
	int numBytes=0;
	const char* retVal = decodeDisasm(DIS_,address,&numBytes);

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

void DisassembleRange(unsigned int start,unsigned int end)
{
	while (start<end)
	{
		start+=Disassemble(start,0);
	}
}	

void CPU_RESET();
int CPU_STEP(int intClocks,int doDebug);
	
int main(int argc,char**argv)
{
	double	atStart,now,remain;
	int a;
	int intClocks=0;
	int curRow=0;
	int numClocks;

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

	AudioInitialise();

	atStart=glfwGetTime();
	//////////////////

	LoadTapeFormat(argv[1]);
	if (InitialiseMemory())
		return -1;
	
	CPU_RESET();
	
//	DisassembleRange(0x0000,0x4000);

	LoadSNA();

	while (1==1)
	{
		static int doDebug=0;
		
		numClocks=CPU_STEP(intClocks,doDebug);	// See cpupin.c or cpusimple.c

		masterClock+=numClocks;
		if (intClocks)
		{
			intClocks-=numClocks;
			if (intClocks<0)
			{
				intClocks=0;
			}
		}

		UpdateTape(numClocks);
		UpdateAudio(numClocks);
		
		if (masterClock>=TSTATES_PER_ROW)
		{	
			pixelClock+=1;
			VID_DrawScreenRow(curRow);
			curRow++;

			masterClock-=TSTATES_PER_ROW;
		}
		if (pixelClock>=ROWS_PER_VBLANK)
		{	
			curRow=0;
			pixelClock-=ROWS_PER_VBLANK;

			intClocks=32;

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
				
			glfwPollEvents();
		
			if (glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
			{
				break;
			}
			if (CheckKey(GLFW_KEY_PAGEUP))
			{
				tapePaused=!tapePaused;
				ClearKey(GLFW_KEY_PAGEUP);
			}
			if (CheckKey(GLFW_KEY_PAGEDOWN))
			{
				syncOff=!syncOff;
				ClearKey(GLFW_KEY_PAGEDOWN);
			}
/*			if (CheckKey(GLFW_KEY_END))
			{
				doDebug=1;
				ClearKey(GLFW_KEY_END);
			}*/
			now=glfwGetTime();

			remain = now-atStart;

			while ((remain<0.02f) && (!syncOff))
			{
				now=glfwGetTime();

				remain = now-atStart;
			}
			atStart=glfwGetTime();
		}
	}

	AudioKill();

	return 0;
}

// Tape handling
uint32_t	tapePos=0;
uint32_t	tapeTCounter=0;
uint8_t		tapeLevel=0;
uint8_t*	tapeFile=NULL;
uint32_t	tapeLength=0;
uint32_t	tapePhase=0;
uint32_t	tapeFormat=0;

uint32_t	tapeDataLength;
uint8_t		tapeDataBit;
uint16_t	tapePilotLength;
uint32_t	tapePilotPulseLength;

uint16_t	tapeParamPilotLength;
uint32_t	tapeParamPilotPulseLength;
uint32_t	tapeParamSync1Length;
uint32_t	tapeParamSync2Length;
uint32_t	tapeParamOneLength;
uint32_t	tapeParamZeroLength;
uint32_t	tapeParamDelayLength;


unsigned char *qLoad(const char *romName,uint32_t *length)
{
	FILE *inRom;
	unsigned char *romData;
	*length=0;

	inRom = fopen(romName,"rb");
	if (!inRom)
	{
		return 0;
	}
	fseek(inRom,0,SEEK_END);
	*length = ftell(inRom);

	fseek(inRom,0,SEEK_SET);
	romData = (unsigned char *)malloc(*length);
	if (romData)
	{
		if ( *length != fread(romData,1,*length,inRom))
		{
			free(romData);
			*length=0;
			romData=0;
		}
	}
	fclose(inRom);

	return romData;
}

int CheckTAP()
{
	uint16_t blockLength;
	uint32_t curPos=0;

	while (1)
	{
		blockLength=tapeFile[curPos++];
		if (curPos>=tapeLength)
		{
			return 0;
		}
		blockLength|=tapeFile[curPos++]<<8;
		if (curPos>=tapeLength)
		{
			return 0;
		}
		if (blockLength==0)
		{
			return 0;	// Not likely to be a .TAP
		}
		if (tapeFile[curPos]<0x80)
		{
			// Header block
			curPos+=19;
			if (curPos==tapeLength)
			{
				return 1;
			}
			if (curPos>tapeLength)
			{
				return 0;
			}
		}
		else
		{
			curPos+=blockLength;
			if (curPos==tapeLength)
			{
				return 1;
			}
			if (curPos>tapeLength)
			{
				return 0;
			}
		}
	}
}

int CheckTZX()
{
	uint32_t a;
	uint32_t blockLength;
	uint32_t curPos=0;

	// Check header
	if (strncmp(tapeFile,"ZXTape!",7)!=0)
	{
		return 0;
	}
	if (tapeFile[7]!=0x1A)
	{
		return 0;
	}

	printf("TZX: Revision : %d.%d\n",tapeFile[8],tapeFile[9]);

	// Quickly check the block Ids - if an unsupported block is found, exit

	curPos=10;
	while (1)
	{
		uint8_t blockId;
		if (curPos>tapeLength)
		{
			printf("Error processing TZX, aborting\n");
			exit(-1);
		}
		if (curPos==tapeLength)
		{
			break;
		}

		blockId=tapeFile[curPos++];
		switch (blockId)
		{
			case 0x10:
				printf("Standard Block\n");
				curPos+=2;
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				curPos+=blockLength;
				break;
			case 0x11:
				printf("Speed Block\n");
				curPos+=15;
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				blockLength|=tapeFile[curPos++]<<16;
				curPos+=blockLength;
				break;
			case 0x14:
				printf("Pure Data\n");
				curPos+=7;
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				blockLength|=tapeFile[curPos++]<<16;
				curPos+=blockLength;
				break;
			case 0x21:
				printf("Group Start\n");
				curPos+=tapeFile[curPos]+1;
				break;
			case 0x22:
				printf("Group End\n");
				break;
			case 0x30:
				printf("Text Description\n");
				curPos+=tapeFile[curPos]+1;
				break;
			case 0x32:
				printf("Archive Info\n");
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				curPos+=blockLength;
				break;
			default:
				printf("Unhandled ID%02X\n",blockId);
				exit(-1);
				break;
		}
	}

	return 1;
}

void LoadTapeFormat(const char* filename)		// Currently handles (raw audio, .TAP (z80) & .tzx formats (tzx implementation incomplete))
{
	if (filename==NULL)
	{
		tapeFormat=99;
		return;
	}

	tapeFile=qLoad(filename,&tapeLength);
	if (!tapeFile)
	{
		printf("Failed to load file - %s (ignoring)\n");
	}

	tapePos=0;
	tapeLevel=0;
	tapePhase=0;
	tapeTCounter=0;

	tapeFormat=0;
	if (CheckTAP())
	{
		tapeFormat=1;
	}
	if (CheckTZX())
	{
		tapePos=10;
		tapeFormat=2;
	}
}

uint8_t GetNextBlockTZX()
{
	uint32_t blockLength;
	uint8_t blockId;
	
	while (1)
	{
		if (tapePos>=tapeLength)
		{
			tapePos=10;
		}

		blockId=tapeFile[tapePos++];
		switch (blockId)
		{
			case 0x10:
				printf("Standard\n");
				tapeParamDelayLength=tapeFile[tapePos++];
				tapeParamDelayLength|=tapeFile[tapePos++]<<8;
				tapeParamDelayLength=tapeParamDelayLength*3494.4f;
				tapeDataLength=tapeFile[tapePos++];
				tapeDataLength|=tapeFile[tapePos++]<<8;
				tapeParamPilotPulseLength=2168;
				if (tapeFile[tapePos]<0x80)
				{
					// Header Pilot Pulse
					tapeParamPilotLength=8063;
				}
				else
				{
					// data Pilot Pulse
					tapeParamPilotLength=3223;
				}
				tapeParamSync1Length=667;
				tapeParamSync2Length=735;
				tapeParamOneLength=1710;
				tapeParamZeroLength=855;
				return 1;
			case 0x11:
				printf("Speed\n");
				tapeParamPilotPulseLength=tapeFile[tapePos++];
				tapeParamPilotPulseLength|=tapeFile[tapePos++]<<8;
				tapeParamSync1Length=tapeFile[tapePos++];
				tapeParamSync1Length|=tapeFile[tapePos++]<<8;
				tapeParamSync2Length=tapeFile[tapePos++];
				tapeParamSync2Length|=tapeFile[tapePos++]<<8;
				tapeParamZeroLength=tapeFile[tapePos++];
				tapeParamZeroLength|=tapeFile[tapePos++]<<8;
				tapeParamOneLength=tapeFile[tapePos++];
				tapeParamOneLength|=tapeFile[tapePos++]<<8;
				tapeParamPilotLength=tapeFile[tapePos++];
				tapeParamPilotLength|=tapeFile[tapePos++]<<8;
				tapePos++;	// Used bits not handled yet
				tapeParamDelayLength=tapeFile[tapePos++];
				tapeParamDelayLength|=tapeFile[tapePos++]<<8;
				tapeParamDelayLength=tapeParamDelayLength*3494.4f;
				tapeDataLength=tapeFile[tapePos++];
				tapeDataLength|=tapeFile[tapePos++]<<8;
				tapeDataLength|=tapeFile[tapePos++]<<16;
				return 1;
			case 0x14:
				printf("Pure Data\n");
				tapeParamZeroLength=tapeFile[tapePos++];
				tapeParamZeroLength|=tapeFile[tapePos++]<<8;
				tapeParamOneLength=tapeFile[tapePos++];
				tapeParamOneLength|=tapeFile[tapePos++]<<8;
				tapePos++;	// Used bits not handled yet
				tapeParamDelayLength=tapeFile[tapePos++];
				tapeParamDelayLength|=tapeFile[tapePos++]<<8;
				tapeParamDelayLength=tapeParamDelayLength*3494.4f;
				tapeDataLength=tapeFile[tapePos++];
				tapeDataLength|=tapeFile[tapePos++]<<8;
				tapeDataLength|=tapeFile[tapePos++]<<16;
				return 4;
			case 0x21:
				printf("Group Start\n");
				tapePos+=tapeFile[tapePos]+1;
				break;
			case 0x22:
				printf("Group End\n");
				break;
			case 0x30:
				printf("Text Description\n");
				tapePos+=tapeFile[tapePos]+1;
				break;
			case 0x32:
				printf("Archive Info\n");
				blockLength=tapeFile[tapePos++];
				blockLength|=tapeFile[tapePos++]<<8;
				tapePos+=blockLength;
				break;
			default:
				printf("UnhandledID \n",blockId);
				break;
		}
	}
}

int GetNextBlockParamsTape()
{
	if (tapeFormat==0)
	{
		return 99;
	}
	if (tapeFormat==1)
	{
		if (tapePos>=tapeLength)
		{
			tapePos=0;
		}
		tapeDataLength=tapeFile[tapePos++];
		tapeDataLength|=tapeFile[tapePos++]<<8;
		tapeParamDelayLength=1*3494.4f;
		tapeParamPilotPulseLength=2168;
		if (tapeFile[tapePos]<0x80)
		{
			// Header Pilot Pulse
			tapeParamPilotLength=8063;
		}
		else
		{
			// data Pilot Pulse
			tapeParamPilotLength=3223;
		}
		tapeParamSync1Length=667;
		tapeParamSync2Length=735;
		tapeParamOneLength=1710;
		tapeParamZeroLength=855;
		return 1;
	}
	if (tapeFormat==2)
	{
		return GetNextBlockTZX(); 
	}
}

void UpdateTape(uint8_t cycles)
{
	int redo=1;
	if (!tapeFile || tapePaused || !cycles)
		return;
			
	while (cycles||redo)
	{
		cycles--;
		redo=0;
		switch (tapePhase)
		{
			case 0:								// New block
				tapePhase = GetNextBlockParamsTape();

				if (tapePhase!=1)
				{
					redo=1;
					break;
				}

				tapePilotLength=tapeParamPilotLength;
				tapePilotPulseLength=tapeParamPilotPulseLength;

				// Fall through - intentional
			case 1:								// Pilot pulse
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapePilotLength--;
					if (tapePilotLength==0)
					{
						tapePhase=2;
						tapePilotPulseLength=tapeParamSync1Length;
					}
				}
				break;
			case 2:								// Sync 1 pulse
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapePhase=3;
					tapePilotPulseLength=tapeParamSync2Length;
				}
				break;
			case 3:								// Sync 2 pulse
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapeDataBit=0x80;
					tapePhase=4;
				}
				break;
			case 4:								// Data
				// For each byte in tapDataLength
				// We get a bit, set the correct pulse length
				// rinse and repeat

				if (tapeFile[tapePos]&tapeDataBit)
				{
					tapePilotPulseLength=tapeParamOneLength;
				}
				else
				{
					tapePilotPulseLength=tapeParamZeroLength;
				}
				tapePhase=5;
				// Fallthrough
			case 5:
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapePhase=6;
				}
				break;
			case 6:
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapeDataBit>>=1;
					tapePhase=4;
					if (tapeDataBit==0)
					{
						tapeDataBit=0x80;
						tapePos++;
						tapeDataLength--;
						if (tapeDataLength==0)
						{
							tapePilotPulseLength=tapeParamDelayLength;
							if (tapePilotPulseLength==0)
							{
								tapePhase=0;
							}
							else
							{
								tapePhase=7;
							}
						}
					}
				}
				break;
			case 7:
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=0;					// After a silence block, level must be 0
					tapePhase=0;
				}
				break;

			case 99:
				tapeTCounter+=4096;
				if (tapeTCounter>=(TSTATES_PER_ROW*ROWS_PER_VBLANK*4096)/((44100)/50))
				{
					tapeTCounter-=(TSTATES_PER_ROW*ROWS_PER_VBLANK*4096)/((44100)/50);
					tapePos++;
					if (tapePos>=tapeLength)
					{
						tapePos=0;
					}

					tapeLevel=tapeFile[tapePos];
				}
				break;
		}
	}
}


uint8_t GetTapeLevel()
{
	return tapeLevel;
}

void LoadSNA()
{
	uint32_t snapLength=0;
	uint8_t *snap=qLoad(".sna",&snapLength);

	if (!snap || snapLength!=49179)
		return;

	IR=(snap[0]<<8)|(snap[20]);
	IFF1=(snap[19]&1);
	IFF2=(snap[19]&2)>>1;
	_HL=(snap[2]<<8)|snap[1];
	_DE=(snap[4]<<8)|snap[3];
	_BC=(snap[6]<<8)|snap[5];
	_AF=(snap[8]<<8)|snap[7];
	HL=(snap[10]<<8)|snap[9];
	DE=(snap[12]<<8)|snap[11];
	BC=(snap[14]<<8)|snap[13];
	AF=(snap[22]<<8)|snap[21];
	IY=(snap[16]<<8)|(snap[15]);
	IX=(snap[18]<<8)|(snap[17]);
	SP=(snap[24]<<8)|(snap[23]);
	IM=snap[25];
	
	memcpy(Ram,&snap[27],16384);
	memcpy(RamExtra,&snap[27+16384],32768);

	PC=GetByte(SP);
	SetByte(SP,0);
	SP++;
	PC|=GetByte(SP)<<8;
	SetByte(SP,0);
	SP++;

	IFF1=IFF2;
}

#define NUMBUFFERS            (3)				/* living dangerously*/

ALuint		  uiBuffers[NUMBUFFERS];
ALuint		  uiSource;

ALboolean ALFWInitOpenAL()
{
	ALCcontext *pContext = NULL;
	ALCdevice *pDevice = NULL;
	ALboolean bReturn = AL_FALSE;
	
	pDevice = alcOpenDevice(NULL);				/* Request default device*/
	if (pDevice)
	{
		pContext = alcCreateContext(pDevice, NULL);
		if (pContext)
		{
			printf("\nOpened %s Device\n", alcGetString(pDevice, ALC_DEVICE_SPECIFIER));
			alcMakeContextCurrent(pContext);
			bReturn = AL_TRUE;
		}
		else
		{
			alcCloseDevice(pDevice);
		}
	}

	return bReturn;
}

ALboolean ALFWShutdownOpenAL()
{
	ALCcontext *pContext;
	ALCdevice *pDevice;

	pContext = alcGetCurrentContext();
	pDevice = alcGetContextsDevice(pContext);
	
	alcMakeContextCurrent(NULL);
	alcDestroyContext(pContext);
	alcCloseDevice(pDevice);

	return AL_TRUE;
}

#if 0/*USE_8BIT_OUTPUT*/

#define AL_FORMAT						(AL_FORMAT_MONO8)
#define BUFFER_FORMAT				U8
#define BUFFER_FORMAT_SIZE	(1)
#define BUFFER_FORMAT_SHIFT	(8)

#else

#define AL_FORMAT						(AL_FORMAT_MONO16)
#define BUFFER_FORMAT				int16_t
#define BUFFER_FORMAT_SIZE	(2)
#define BUFFER_FORMAT_SHIFT	(0)

#endif

int curPlayBuffer=0;

#define BUFFER_LEN		(44100/50)

BUFFER_FORMAT audioBuffer[BUFFER_LEN];
int amountAdded=0;

void AudioInitialise()
{
	int a=0;
	for (a=0;a<BUFFER_LEN;a++)
	{
		audioBuffer[a]=0;
	}

	ALFWInitOpenAL();

  /* Generate some AL Buffers for streaming */
	alGenBuffers( NUMBUFFERS, uiBuffers );

	/* Generate a Source to playback the Buffers */
  alGenSources( 1, &uiSource );

	for (a=0;a<NUMBUFFERS;a++)
	{
		alBufferData(uiBuffers[a], AL_FORMAT, audioBuffer, BUFFER_LEN*BUFFER_FORMAT_SIZE, 44100);
		alSourceQueueBuffers(uiSource, 1, &uiBuffers[a]);
	}

	alSourcePlay(uiSource);
}


void AudioKill()
{
	ALFWShutdownOpenAL();
}

int16_t currentDAC[2] = {0,0};

void _AudioAddData(int channel,int16_t dacValue)
{
	currentDAC[channel]=dacValue;
}

uint32_t tickCnt=0;
uint32_t tickRate=((TSTATES_PER_ROW*ROWS_PER_VBLANK*4096)/(44100/50));

/* audio ticked at same clock as everything else... so need a step down */
void UpdateAudio(int numClocks)
{
	tickCnt+=numClocks*4096;
	
	if (tickCnt>=tickRate*50)
	{
		tickCnt-=tickRate;

		if (amountAdded!=BUFFER_LEN)
		{
			int32_t res=0;
			res+=currentDAC[0];
			res+=GetTapeLevel()?4095:-4096;
			res>>=1;
			audioBuffer[amountAdded]=res>>BUFFER_FORMAT_SHIFT;
			amountAdded++;
		}
	}

	if (amountAdded==BUFFER_LEN)
	{
		/* 1 second has passed by */

		ALint processed;
		ALint state;
		alGetSourcei(uiSource, AL_SOURCE_STATE, &state);
		alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &processed);
		if (processed>0)
		{
			ALuint buffer;

			amountAdded=0;
			alSourceUnqueueBuffers(uiSource,1, &buffer);
			alBufferData(buffer, AL_FORMAT, audioBuffer, BUFFER_LEN*BUFFER_FORMAT_SIZE, 44100);
			alSourceQueueBuffers(uiSource, 1, &buffer);
		}

		if (state!=AL_PLAYING)
		{
			alSourcePlay(uiSource);
		}
	}
}
