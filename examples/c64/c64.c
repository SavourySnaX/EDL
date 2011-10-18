/*
 * C64 implementation
 *
 *
 *	Currently using pure 6502 - so port mappings are emulated in the memory mapper
 *
 *
 *
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

#include "gui\debugger.h"

#define DEBUG_WINDOWS	1

int DISK_InitialiseMemory();
void DISK_Reset();
uint16_t DISK_Tick(uint8_t* clk,uint8_t* atn,uint8_t* data);
uint8_t DISK_GetByte(uint16_t addr);

void AudioKill();
void AudioInitialise();
void UpdateAudio();
void _AudioAddData(int channel,int16_t dacValue);

void Reset6581();
void SetByte6581(uint16_t regNo,uint8_t byte);
void Tick6581();

uint16_t MAIN_PinGetPIN_AB();
uint8_t MAIN_PinGetPIN_DB();
void MAIN_PinSetPIN_DB(uint8_t);
void MAIN_PinSetPIN_O0(uint8_t);
void MAIN_PinSetPIN_SO(uint8_t);
uint8_t MAIN_PinGetPIN_SYNC();
uint8_t MAIN_PinGetPIN_RW();
void MAIN_PinSetPIN__IRQ(uint8_t);
void MAIN_PinSetPIN__RES(uint8_t);

// Step 1. Memory

unsigned char CRom[0x1000];
unsigned char BRom[0x2000];
unsigned char KRom[0x2000];

unsigned char Ram[0x10000];

unsigned char CRam[0x400];

unsigned char* testOverlay;//[0x4000];


int playDown=0;
int recDown=0;

uint8_t M6569_IRQ=1;

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
	if (LoadRom(CRom,0x1000,"roms/chargen"))
		return 1;
	if (LoadRom(BRom,0x2000,"roms/basic"))
		return 1;
	if (LoadRom(KRom,0x2000,"roms/kernal"))
		return 1;

	return 0;
}

#if 0
uint8_t VIAGetByte(int chipNo,int regNo);
void VIASetByte(int chipNo,int regNo,uint8_t byte);
void VIATick(int chipNo,uint8_t PB0);
#endif

uint8_t GetByte6569(int regNo);
void SetByte6569(int regNo,uint8_t byte);
void Tick6569();

uint8_t lastBus=0xFF;

int doDebug=0;

uint8_t M6510_DDR=0xEF;
uint8_t M6510_PCR=0xFF;		//6-7xxx  5-cas motor  4-cas switch  3-cas out  2-charen  1-hiram   0-loram

uint8_t PLA_GAME=0x1;
uint8_t PLA_EXROM=0x1;

typedef uint8_t	(*ReadMap)(uint16_t);
typedef void	(*WriteMap)(uint16_t,uint8_t);

void WriteRam(uint16_t addr,uint8_t byte)
{
	Ram[addr]=byte;
}

void WriteIO(uint16_t addr,uint8_t byte)
{
	if (addr<0xD400)
	{
		SetByte6569(addr&0x3F,byte);
		return;
	}
	if (addr<0xD800)
	{
		SetByte6581(addr&0x1F,byte);
		return;
	}
	if (addr<0xDC00)
	{
		CRam[addr-0xD800]=byte&0xF;
		return;
	}
	if (addr<0xDD00)
	{
		CIA0_PinSetPIN_DB(byte);
		return;
	}
	if (addr<0xDE00)
	{
		CIA1_PinSetPIN_DB(byte);
		return;
	}
//	printf("IO Expansion\n");
	return;
}

void WriteRomL(uint16_t addr,uint8_t byte)
{
// TODO: May need to write to cart ram
}

void WriteRomH(uint16_t addr,uint8_t byte)
{
// TODO: May need to write to cart ram
}

void WriteOpen(uint16_t addr,uint8_t byte)
{
// TODO: Need to pass through blocks to cartridge (if writeable connection)
}

uint8_t ReadIO(uint16_t addr)
{
	if (addr<0xD400)
	{
		return GetByte6569(addr&0x3F);
	}
	if (addr<0xD800)
	{
		//				printf("SID Register Read : %02X  ($1D-$1F - unconnected)\n",addr&0x1F);
		return 0xFF;
	}
	if (addr<0xDC00)
	{
		return CRam[addr-0xD800]&0xF;
	}
	if (addr<0xDD00)
	{
		return CIA0_PinGetPIN_DB();
	}
	if (addr<0xDE00)
	{
		return CIA1_PinGetPIN_DB();
	}
//	printf("IO Expansion\n");
	return 0xFF;
}

uint8_t ReadRam(uint16_t addr)
{
	return Ram[addr];
}

uint8_t ReadChar(uint16_t addr)
{
	return CRom[addr&0x0FFF];
}

uint8_t ReadBasic(uint16_t addr)
{
	return BRom[addr&0x1FFF];
}

uint8_t ReadKernel(uint16_t addr)
{
	return KRom[addr&0x1FFF];
}

uint8_t ReadCartL(uint16_t addr)
{
	return testOverlay[addr&0x1FFF];		// NB: May need clamping for 4k roms.. expanding for >8k??
}

uint8_t ReadCartH(uint16_t addr)
{
	return testOverlay[0x2000 + (addr&0x1FFF)];	// NB: May need clamping for 4k roms.. expanding for >8k??
}

uint8_t ReadOpen(uint16_t addr)
{
//TODO: need to query cartridge address bus
	return 0xFF;
}

WriteMap writeIO[16]={WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteIO,WriteRam,WriteRam};
WriteMap writeRam[16]={WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam,WriteRam};
WriteMap writeOpen[16]={WriteRam,WriteOpen,WriteOpen,WriteOpen,WriteOpen,WriteOpen,WriteOpen,WriteOpen,WriteRomL,WriteRomL,WriteOpen,WriteOpen,WriteOpen,WriteIO,WriteRomH,WriteRomH};

ReadMap readChar[16]={ ReadRam,ReadChar,ReadRam,ReadRam, ReadRam,ReadRam,ReadRam,ReadRam, ReadRam,ReadChar,ReadRam,ReadRam, ReadRam,ReadRam,ReadRam,ReadRam};
ReadMap readRomH[16]={ReadRam,ReadRam,ReadRam,ReadCartH,ReadRam,ReadRam,ReadRam,ReadCartH,ReadRam,ReadRam,ReadRam,ReadCartH,ReadRam,ReadRam,ReadRam,ReadCartH};

ReadMap readRam[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam};
ReadMap readI[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadIO,ReadRam,ReadRam};
ReadMap readC[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadChar,ReadRam,ReadRam};
ReadMap readIK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadIO,ReadKernel,ReadKernel};
ReadMap readCK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadChar,ReadKernel,ReadKernel};
ReadMap readHIK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadCartH,ReadCartH,ReadRam,ReadIO,ReadKernel,ReadKernel};
ReadMap readHCK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadCartH,ReadCartH,ReadRam,ReadChar,ReadKernel,ReadKernel};
ReadMap readBIK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadBasic,ReadBasic,ReadRam,ReadIO,ReadKernel,ReadKernel};
ReadMap readBCK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadBasic,ReadBasic,ReadRam,ReadChar,ReadKernel,ReadKernel};
ReadMap readLBIK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadCartL,ReadCartL,ReadBasic,ReadBasic,ReadRam,ReadIO,ReadKernel,ReadKernel};
ReadMap readLBCK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadCartL,ReadCartL,ReadBasic,ReadBasic,ReadRam,ReadChar,ReadKernel,ReadKernel};
ReadMap readLHIK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadCartL,ReadCartL,ReadCartH,ReadCartH,ReadRam,ReadIO,ReadKernel,ReadKernel};
ReadMap readLHCK[16]={ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadRam,ReadCartL,ReadCartL,ReadCartH,ReadCartH,ReadRam,ReadChar,ReadKernel,ReadKernel};
ReadMap readOpen[16]={ReadRam,ReadOpen,ReadOpen,ReadOpen,ReadOpen,ReadOpen,ReadOpen,ReadOpen,ReadCartL,ReadCartL,ReadOpen,ReadOpen,ReadOpen,ReadIO,ReadCartH,ReadCartH};

ReadMap *r_memMap=readBIK;
ReadMap *v_memMap=readChar;
WriteMap *w_memMap=writeIO;

uint8_t lastMemControl=0x1F;

void ChangeMemoryMap()
{
	int a;
	uint8_t curMemControl=M6510_PCR&0x07;
	curMemControl|=PLA_GAME<<4;
	curMemControl|=PLA_EXROM<<3;

	if (curMemControl==lastMemControl)
		return;

	lastMemControl=curMemControl;

	switch (curMemControl)
	{
		case 0x1F:
			w_memMap=writeIO;
			r_memMap=readBIK;
			v_memMap=readChar;
			break;
		case 0x1B:
			w_memMap=writeRam;
			r_memMap=readBCK;
			v_memMap=readChar;
			break;
		case 0x17:
			w_memMap=writeIO;
			r_memMap=readLBIK;
			v_memMap=readChar;
			break;
		case 0x13:
			w_memMap=writeRam;
			r_memMap=readLBCK;
			v_memMap=readChar;
			break;
		case 0x07:
			w_memMap=writeIO;
			r_memMap=readLHIK;
			v_memMap=readChar;
			break;
		case 0x03:
			w_memMap=writeRam;
			r_memMap=readLHCK;
			v_memMap=readChar;
			break;
		case 0x06:
			w_memMap=writeIO;
			r_memMap=readHIK;
			v_memMap=readChar;
			break;
		case 0x02:
			w_memMap=writeIO;
			r_memMap=readHCK;
			v_memMap=readChar;
			break;
		case 0x1E:
		case 0x16:
			w_memMap=writeIO;
			r_memMap=readIK;
			v_memMap=readChar;
			break;
		case 0x1A:
		case 0x12:
			w_memMap=writeRam;
			r_memMap=readCK;
			v_memMap=readChar;
			break;
		case 0x1D:
		case 0x15:
		case 0x05:
			w_memMap=writeIO;
			r_memMap=readI;
			v_memMap=readChar;
			break;
		case 0x19:
		case 0x11:
			w_memMap=writeRam;
			r_memMap=readC;
			v_memMap=readChar;
			break;
		case 0x1C:
		case 0x18:
		case 0x14:
		case 0x10:
		case 0x04:
		case 0x00:
		case 0x01:
			w_memMap=writeRam;
			r_memMap=readRam;
			v_memMap=readChar;
			break;
		case 0x0F:
		case 0x0E:
		case 0x0D:
		case 0x0C:
		case 0x0B:
		case 0x0A:
		case 0x09:
		case 0x08:
			w_memMap=writeOpen;
			r_memMap=readOpen;
			v_memMap=readRomH;
			break;
	}
}

uint8_t GetByte(uint16_t addr)
{
	uint8_t map=addr>>12;

	if (addr==0x0000)
		return M6510_DDR;
	if (addr==0x0001)
		return M6510_PCR;

	return r_memMap[map](addr);
}

void SetByte(uint16_t addr,uint8_t byte)
{
	uint8_t map=addr>>12;

	if (addr==0x0000)
	{
		M6510_DDR=byte;
		return;
	}
	if (addr==0x0001)
	{
		M6510_PCR&=~M6510_DDR;
		byte&=M6510_DDR;
		M6510_PCR|=byte;
		ChangeMemoryMap();
		return;
	}

	w_memMap[map](addr,byte);
}


int masterClock=0;
int pixelClock=0;
int cpuClock=0;

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

extern uint8_t *MAIN_DIS_[256];

extern uint8_t	MAIN_A;
extern uint8_t	MAIN_X;
extern uint8_t	MAIN_Y;
extern uint16_t	MAIN_SP;
extern uint16_t	MAIN_PC;
extern uint8_t	MAIN_P;

void DUMP_REGISTERS()
{
	printf("--------\n");
	printf("FLAGS = N  V  -  B  D  I  Z  C\n");
	printf("        %s  %s  %s  %s  %s  %s  %s  %s\n",
			MAIN_P&0x80 ? "1" : "0",
			MAIN_P&0x40 ? "1" : "0",
			MAIN_P&0x20 ? "1" : "0",
			MAIN_P&0x10 ? "1" : "0",
			MAIN_P&0x08 ? "1" : "0",
			MAIN_P&0x04 ? "1" : "0",
			MAIN_P&0x02 ? "1" : "0",
			MAIN_P&0x01 ? "1" : "0");
	printf("A = %02X\n",MAIN_A);
	printf("X = %02X\n",MAIN_X);
	printf("Y = %02X\n",MAIN_Y);
	printf("SP= %04X\n",MAIN_SP);
#if 0
	printf("VIA1 IFR/IER/ACR/PCR/T1C/T2C = %02X/%02X/%02X/%02X/%04X/%04X\n",via[0].IFR,via[0].IER,via[0].ACR,via[0].PCR,via[0].T1C,via[0].T2C);
	printf("VIA2 IFR/IER/ACR/PCR/T1C/T2C = %02X/%02X/%02X/%02X/%04X/%04X\n",via[1].IFR,via[1].IER,via[1].ACR,via[1].PCR,via[1].T1C,via[1].T2C);
#endif
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
	const char* retVal = decodeDisasm(MAIN_DIS_,address,&numBytes,255);

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

#define VIA_WIDTH	700
#define	VIA_HEIGHT	200

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	(256+24)

#define MEMORY_WIDTH	450
#define	MEMORY_HEIGHT	256

#define TIMING_WIDTH	640*2
#define TIMING_HEIGHT	280

#define HEIGHT	(312)
#define	WIDTH	(63*8)

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define TIMING_WINDOW		1
#define REGISTER_WINDOW		2
#define REGISTER_WINDOW_DISK	3
#define VIA_WINDOW		4
#define VIA_WINDOW_DISK		5
#define MEMORY_WINDOW		6
#define REGISTER_6569		7

#define ENABLE_DISK_DEBUG	0

GLFWwindow windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

void ShowScreen(int windowNum,int w,int h)
{
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f,1.0f);

	glTexCoord2f(0.0f, h);
	glVertex2f(-1.0f, -1.0f);

	glTexCoord2f(w, h);
	glVertex2f(1.0f, -1.0f);

	glTexCoord2f(w, 0.0f);
	glVertex2f(1.0f, 1.0f);
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

struct KeyArray
{
	unsigned char lastState;
	unsigned char curState;
	unsigned char debounced;
	GLFWwindow    window;
};

struct KeyArray keyArray[512];

int KeyDown(int key)
{
	return keyArray[key].curState==GLFW_PRESS;
}

int CheckKey(int key)
{
	return keyArray[key].debounced;
}

int CheckKeyWindow(int key,GLFWwindow window)
{
	return keyArray[key].debounced && (keyArray[key].window==window);
}

void ClearKey(int key)
{
	keyArray[key].debounced=0;
}

void kbHandler( GLFWwindow window, int key, int action )
{
	keyArray[key].lastState=keyArray[key].curState;
	keyArray[key].curState=action;
	keyArray[key].debounced|=(keyArray[key].lastState==GLFW_RELEASE)&&(keyArray[key].curState==GLFW_PRESS);
	keyArray[key].window=window;
}

void sizeHandler(GLFWwindow window,int xs,int ys)
{
	glfwMakeContextCurrent(window);
	glViewport(0, 0, xs, ys);
}

void AttachImage(const char* fileName);
void SaveTAP(const char* filename);

uint8_t CheckKeys(int key0,int key1,int key2,int key3,int key4,int key5,int key6,int key7)
{
	uint8_t keyVal=0xFF;

	if (KeyDown(key0))
	{
		keyVal&=~0x01;
	}
	if (KeyDown(key1))
	{
		keyVal&=~0x02;
	}
	if (KeyDown(key2))
	{
		keyVal&=~0x04;
	}
	if (KeyDown(key3))
	{
		keyVal&=~0x08;
	}
	if (KeyDown(key4))
	{
		keyVal&=~0x10;
	}
	if (KeyDown(key5))
	{
		keyVal&=~0x20;
	}
	if (KeyDown(key6))
	{
		keyVal&=~0x40;
	}
	if (KeyDown(key7))
	{
		keyVal&=~0x80;
	}

	return keyVal;
}

uint8_t kbuffer[8]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void UpdateKB()
{
	int colNum=0;
	uint8_t keyval=0xFF;
	uint8_t column=~CIA0_PinGetPIN_PA();

	while (column)
	{
		if (column&1)
		{
			keyval&=kbuffer[colNum];
		}
		colNum++;
		column>>=1;
	}
	
	if (keyval!=0xFF)
	{
//		doDebug=1;
	}

	CIA0_PinSetPIN_PB(keyval);
}

void UpdateJoy()		// For now returns data on BOTH ports
{
	uint8_t joyVal=0;
	joyVal|=KeyDown(GLFW_KEY_KP_8)?1:0;
	joyVal|=KeyDown(GLFW_KEY_KP_2)?2:0;
	joyVal|=KeyDown(GLFW_KEY_KP_4)?4:0;
	joyVal|=KeyDown(GLFW_KEY_KP_6)?8:0;
	joyVal|=KeyDown(GLFW_KEY_KP_0)?0x10:0;

	if (joyVal)
	{
		uint8_t prv;
		prv=CIA0_PinGetPIN_PA();
		prv&=0xE0;
		prv|=~joyVal;
		CIA0_PinSetPIN_PA(prv);
		prv=CIA0_PinGetPIN_PB();
		prv&=0xE0;
		prv|=~joyVal;
		CIA0_PinSetPIN_PB(prv);
	}
}

int casLevel=0;
uint32_t casCount;

#define INTERNAL_TAPE_BUFFER_SIZE		(1024*1024*10)

uint32_t cntBuffer[INTERNAL_TAPE_BUFFER_SIZE];
uint32_t cntPos=0;
uint32_t cntMax=0;


void UpdateCassette()
{
#if 1
	if (CheckKey(GLFW_KEY_KP_ADD))
	{
		playDown=1;
		recDown=playDown;
		ClearKey(GLFW_KEY_KP_ADD);
		cntPos=0;
		casCount=0;
		casLevel=0;		
	}
#endif
	if (CheckKey(GLFW_KEY_PAGEDOWN))
	{
		playDown=1;
		recDown=0;
		ClearKey(GLFW_KEY_PAGEDOWN);
		cntPos=0;
		casCount=0;
		casLevel=0;
	}
	if (CheckKey(GLFW_KEY_PAGEUP))
	{
		playDown=0;
		recDown=0;
		ClearKey(GLFW_KEY_PAGEUP);
		cntPos=0;
		casCount=0;
		casLevel=0;		
	}

	// Cassette deck
	
	M6510_PCR&=0xEF;
	M6510_PCR|=(playDown<<4)^0x10;
#if 1
	if (playDown && recDown && (M6510_PCR&0x20)==0) // SAVING
	{
		uint8_t newRecLevel = (M6510_PCR&0x08)>>3;
		casCount++;
		if (newRecLevel!=casLevel)
		{
			//printf("Level Changed %08X\n",casCount);
			cntMax=cntPos;
			cntBuffer[cntPos++]=casCount;
			casCount=0;
			casLevel=newRecLevel;
		}
//		RecordPin(8,newRecLevel);
	}
#endif
	if (playDown && (!recDown) && (M6510_PCR&0x20)==0) // LOADING
	{
		casCount++;
		if (casCount>=cntBuffer[cntPos])
		{
			casLevel^=1;
			cntPos++;
			casCount=0;
		}
		CIA0_PinSetPIN__FLAG(casLevel);
	}

	RecordPin(7,casLevel);
}

uint8_t lastClk=12,lastDat=12;

uint16_t DISK_lastPC;

void UpdateDiskInterface()
{
	uint8_t tmp,newpa;
	uint8_t clk,dat,atn;
	uint8_t clko,dato;

	atn=(CIA1_PinGetPIN_PA()&0x08)>>3;
	clk=(CIA1_PinGetPIN_PA()&0x10)>>4;
	dat=(CIA1_PinGetPIN_PA()&0x20)>>5;

	clk^=1;
	dat^=1;
	atn^=1;

	clko=clk;
	dato=dat;

	RecordPin(0,atn);
	RecordPin(1,clk);
	RecordPin(2,dat);

	DISK_lastPC=DISK_Tick(&clk,&atn,&dat);
	
	RecordPin(3,clk);
	RecordPin(4,dat);
	RecordPin(5,atn);

	UpdatePinTick();

	tmp=CIA1_PinGetPIN_PA()&0x3F;
	tmp|=clk<<6;
	tmp|=dat<<7;

	CIA1_PinSetPIN_PA(tmp);
}

uint16_t g_whereToLook=1;

void UpdateHardware()
{
	UpdateKB();
	UpdateJoy();
	UpdateCassette();
	UpdateDiskInterface();
}
		
int stopTheClock=0;

int main(int argc,char**argv)
{
	int w,h;
	double	atStart,now,remain;
	uint16_t bp;

	/// Initialize GLFW 
	glfwInit(); 

#if DEBUG_WINDOWS
	// Open memory OpenGL window 
	if( !(windows[MEMORY_WINDOW]=glfwOpenWindow( MEMORY_WIDTH, MEMORY_HEIGHT, GLFW_WINDOWED,"Memory",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MEMORY_WINDOW],0,0);

	glfwMakeContextCurrent(windows[MEMORY_WINDOW]);
	setupGL(MEMORY_WINDOW,MEMORY_WIDTH,MEMORY_HEIGHT);
	
	// Open timing OpenGL window 
	if( !(windows[TIMING_WINDOW]=glfwOpenWindow( TIMING_WIDTH, TIMING_HEIGHT, GLFW_WINDOWED,"Serial Bus",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[TIMING_WINDOW],0,640);

	glfwMakeContextCurrent(windows[TIMING_WINDOW]);
	setupGL(TIMING_WINDOW,TIMING_WIDTH,TIMING_HEIGHT);

#if ENABLE_DISK_DEBUG
	// Open via OpenGL window 
	if( !(windows[VIA_WINDOW_DISK]=glfwOpenWindow( VIA_WIDTH, VIA_HEIGHT, GLFW_WINDOWED,"VIA Chips (disk)",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[VIA_WINDOW_DISK],560,640);

	glfwMakeContextCurrent(windows[VIA_WINDOW_DISK]);
	setupGL(VIA_WINDOW_DISK,VIA_WIDTH,VIA_HEIGHT);
	
	// Open registers OpenGL window 
	if( !(windows[REGISTER_WINDOW_DISK]=glfwOpenWindow( REGISTER_WIDTH, REGISTER_HEIGHT, GLFW_WINDOWED,"Disk CPU",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[REGISTER_WINDOW_DISK],900,300);

	glfwMakeContextCurrent(windows[REGISTER_WINDOW_DISK]);
	setupGL(REGISTER_WINDOW_DISK,REGISTER_WIDTH,REGISTER_HEIGHT);
#endif

	// Open via OpenGL window 
	if( !(windows[REGISTER_6569]=glfwOpenWindow( REGISTER_WIDTH, REGISTER_HEIGHT, GLFW_WINDOWED,"6569",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[REGISTER_6569],0,0);

	glfwMakeContextCurrent(windows[REGISTER_6569]);
	setupGL(REGISTER_6569,REGISTER_WIDTH,REGISTER_HEIGHT);
	
	// Open via OpenGL window 
	if( !(windows[VIA_WINDOW]=glfwOpenWindow( VIA_WIDTH, VIA_HEIGHT, GLFW_WINDOWED,"CIA Chips (c64)",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[VIA_WINDOW],560,0);

	glfwMakeContextCurrent(windows[VIA_WINDOW]);
	setupGL(VIA_WINDOW,VIA_WIDTH,VIA_HEIGHT);
	
	// Open registers OpenGL window 
	if( !(windows[REGISTER_WINDOW]=glfwOpenWindow( REGISTER_WIDTH, REGISTER_HEIGHT, GLFW_WINDOWED,"Main CPU",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[REGISTER_WINDOW],600,300);

	glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
	setupGL(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);
#endif
	// Open screen OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwOpenWindow( WIDTH, HEIGHT, GLFW_WINDOWED,"c64",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],0,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC

	glfwGetWindowSize(windows[MAIN_WINDOW],&w,&h);

	//printf("width : %d (%d) , height : %d (%d)\n", w,WIDTH,h,HEIGHT);
	glfwSetKeyCallback(kbHandler);
	glfwSetWindowSizeCallback(sizeHandler);

	atStart=glfwGetTime();
	//////////////////

	if (InitialiseMemory())
		return -1;
	if (DISK_InitialiseMemory())
		return -1;

	if (argc==2)
	{
		AttachImage(argv[1]);
	}

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
	MAIN_PinSetPIN_SO(1);
	MAIN_PinSetPIN__IRQ(1);

	//dumpInstruction=100000;

	MAIN_PC=GetByte(0xFFFC);
	MAIN_PC|=GetByte(0xFFFD)<<8;

	bp = GetByte(0xc000);
	bp|=GetByte(0xC001)<<8;
	bp = GetByte(0xFFFE);
	bp|=GetByte(0xFFFF)<<8;

	//printf("%04X\n",bp);

	//printf("%02X != %02X\n",BRom[0xD487-0xC000],GetByte(0xD487));
	
	DISK_Reset();
	Reset6581();
	
#if 0
	uint8_t pb0=0xFF;
	via[0].CB1=1;
#endif

	CIA0_PinSetPIN__RES(0);
	CIA0_PinSetPIN_O2(0);
	CIA0_PinSetPIN_O2(1);
	CIA0_PinSetPIN_O2(0);
	CIA0_PinSetPIN_O2(1);
	CIA0_PinSetPIN_O2(0);
	CIA0_PinSetPIN__RES(1);

	CIA1_PinSetPIN__RES(0);
	CIA1_PinSetPIN_O2(0);
	CIA1_PinSetPIN_O2(1);
	CIA1_PinSetPIN_O2(0);
	CIA1_PinSetPIN_O2(1);
	CIA1_PinSetPIN_O2(0);
	CIA1_PinSetPIN__RES(1);

	AudioInitialise();
	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_F12))
	{
		static uint16_t lastPC;
		static uint8_t lastPb0=0xff;
		uint16_t addr; 
		
		if ((!stopTheClock) || g_traceStep || g_instructionStep)
		{
			Tick6569();
			MAIN_PinSetPIN_O0(1);
			addr = MAIN_PinGetPIN_AB();

			if (MAIN_PinGetPIN_SYNC())
			{
				lastPC=addr;

				if (isBreakpoint(0,lastPC))
				{
					stopTheClock=1;
				}


				//			if (MAIN_PinGetPIN_AB()==bp)
				//				doDebug=1;

				if (doDebug)
				{
					Disassemble(addr,1);
					getch();
				}
			}

			CIA0_PinSetPIN_RW(MAIN_PinGetPIN_RW());
			CIA1_PinSetPIN_RW(MAIN_PinGetPIN_RW());

			CIA0_PinSetPIN_RS(addr&0xF);
			CIA1_PinSetPIN_RS(addr&0xF);

			if ((addr&0xFE00)==0xDC00)
			{
				CIA0_PinSetPIN__CS((addr&0x0100)>>8);
				CIA1_PinSetPIN__CS(((~addr)&0x0100)>>8);
			}
			else
			{
				CIA0_PinSetPIN__CS(1);
				CIA1_PinSetPIN__CS(1);
			}

			CIA0_PinSetPIN_O2(1);
			CIA1_PinSetPIN_O2(1);

			if (MAIN_PinGetPIN_RW())
			{
				uint8_t  data = GetByte(addr);
				if (doDebug)
					printf("Getting : %02X @ %04X\n", data,addr);
				MAIN_PinSetPIN_DB(data);
			}
			if (!MAIN_PinGetPIN_RW())
			{
				if (doDebug)
					printf("Storing : %02X @ %04X\n", MAIN_PinGetPIN_DB(),addr);
				SetByte(addr,MAIN_PinGetPIN_DB());
			}

			CIA0_PinSetPIN_O2(0);
			CIA1_PinSetPIN_O2(0);

			UpdateHardware();

			MAIN_PinSetPIN__IRQ(CIA0_PinGetPIN__IRQ()&M6569_IRQ);

			lastBus=MAIN_PinGetPIN_DB();

			MAIN_PinSetPIN_O0(0);
			Tick6569();
			Tick6581();
			UpdateAudio();


			pixelClock++;
		}

		if (pixelClock>=(63*312) || stopTheClock)
		{
			static int normalSpeed=1;

			if (pixelClock>=(63*312))
			{
				CIA0_PinSetPIN_TOD(1);
				CIA0_PinSetPIN_TOD(0);
				CIA1_PinSetPIN_TOD(1);
				CIA1_PinSetPIN_TOD(0);
				pixelClock-=(63*312);
			}

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
#if DEBUG_WINDOWS
			glfwMakeContextCurrent(windows[MEMORY_WINDOW]);
			DrawMemory(videoMemory[MEMORY_WINDOW],MEMORY_WIDTH,g_whereToLook,ReadRam);
			ShowScreen(MEMORY_WINDOW,MEMORY_WIDTH,MEMORY_HEIGHT);
			glfwSwapBuffers();

			glfwMakeContextCurrent(windows[TIMING_WINDOW]);
			DrawTimingA(videoMemory[TIMING_WINDOW],TIMING_WIDTH);
			UpdateTimingWindow(windows[TIMING_WINDOW]);
			ShowScreen(TIMING_WINDOW,TIMING_WIDTH,TIMING_HEIGHT);
			glfwSwapBuffers();
#if ENABLE_DISK_DEBUG
			glfwMakeContextCurrent(windows[VIA_WINDOW_DISK]);
			DrawVIADisk(videoMemory[VIA_WINDOW_DISK],VIA_WIDTH);
			ShowScreen(VIA_WINDOW_DISK,VIA_WIDTH,VIA_HEIGHT);
			glfwSwapBuffers();
			
			glfwMakeContextCurrent(windows[REGISTER_WINDOW_DISK]);
			DrawRegisterDisk(videoMemory[REGISTER_WINDOW_DISK],REGISTER_WIDTH,DISK_lastPC,DISK_GetByte);
			UpdateRegisterDisk(windows[REGISTER_WINDOW_DISK]);
			ShowScreen(REGISTER_WINDOW_DISK,REGISTER_WIDTH,REGISTER_HEIGHT);
			glfwSwapBuffers();
#endif
			glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
			DrawRegisterMain(videoMemory[REGISTER_WINDOW],REGISTER_WIDTH,lastPC,GetByte);
			UpdateRegisterMain(windows[REGISTER_WINDOW]);
			ShowScreen(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);
			glfwSwapBuffers();
			
			glfwMakeContextCurrent(windows[VIA_WINDOW]);
			DrawVIAMain(videoMemory[VIA_WINDOW],VIA_WIDTH);
			ShowScreen(VIA_WINDOW,VIA_WIDTH,VIA_HEIGHT);
			glfwSwapBuffers();
			
			glfwMakeContextCurrent(windows[REGISTER_6569]);
			DrawRegister6569(videoMemory[REGISTER_6569],REGISTER_WIDTH);
			ShowScreen(REGISTER_6569,REGISTER_WIDTH,REGISTER_HEIGHT);
			glfwSwapBuffers();
#endif
			glfwPollEvents();
			
#if 0
			kbuffer[0]=CheckKeys('1','3','5','7','9','-','=',GLFW_KEY_BACKSPACE);
			kbuffer[1]=CheckKeys('`','W','R','Y','I','P',']',GLFW_KEY_ENTER);
			kbuffer[2]=CheckKeys(GLFW_KEY_LCTRL,'A','D','G','J','L','\'',GLFW_KEY_RIGHT);
			kbuffer[3]=CheckKeys(GLFW_KEY_TAB,GLFW_KEY_LSHIFT,'X','V','N',',','/',GLFW_KEY_DOWN);
			kbuffer[4]=CheckKeys(GLFW_KEY_SPACE,'Z','C','B','M','.',GLFW_KEY_RSHIFT,GLFW_KEY_F1);
			kbuffer[5]=CheckKeys(GLFW_KEY_RCTRL,'S','F','H','K',';','#',GLFW_KEY_F3);
			kbuffer[6]=CheckKeys('Q','E','T','U','O','[','/',GLFW_KEY_F5);
			kbuffer[7]=CheckKeys('2','4','6','8','0','\\',GLFW_KEY_HOME,GLFW_KEY_F7);
#else
			kbuffer[0]=CheckKeys(GLFW_KEY_BACKSPACE,GLFW_KEY_ENTER,GLFW_KEY_RIGHT,GLFW_KEY_F7,GLFW_KEY_F1,GLFW_KEY_F3,GLFW_KEY_F5,GLFW_KEY_DOWN); 
			kbuffer[1]=CheckKeys('3','W','A','4','Z','S','E',GLFW_KEY_LSHIFT);
			kbuffer[2]=CheckKeys('5','R','D','6','C','F','T','X');
			kbuffer[3]=CheckKeys('7','Y','G','8','B','H','U','V');
			kbuffer[4]=CheckKeys('9','I','J','0','M','K','O','N');
			kbuffer[5]=CheckKeys('-','P','L','=','.',';','[',',');
			kbuffer[6]=CheckKeys(GLFW_KEY_INSERT,']','\'',GLFW_KEY_HOME,GLFW_KEY_RSHIFT,'#',GLFW_KEY_DELETE,'/');
			kbuffer[7]=CheckKeys('1','`',GLFW_KEY_TAB,'2',GLFW_KEY_SPACE,GLFW_KEY_LCTRL,'Q',GLFW_KEY_ESC);

#endif
			if (CheckKey(GLFW_KEY_END))
			{
				ClearKey(GLFW_KEY_END);
				normalSpeed^=1;
			}
			if (CheckKey(GLFW_KEY_KP_SUBTRACT))
			{
				ClearKey(GLFW_KEY_KP_SUBTRACT);
				stopTheClock^=1;
			}
			g_traceStep=0;
			if (KeyDown(GLFW_KEY_KP_DIVIDE))
			{
				g_traceStep=1;
			}
			if (CheckKey(GLFW_KEY_KP_MULTIPLY))
			{
				ClearKey(GLFW_KEY_KP_MULTIPLY);
				g_traceStep=1;
			}
			
			now=glfwGetTime();

			remain = now-atStart;

			while ((remain<0.02f) && normalSpeed && !stopTheClock)
			{
				now=glfwGetTime();

				remain = now-atStart;
			}
			atStart=glfwGetTime();
		}
	}

	SaveTAP("test.tap");
	
	AudioKill();

	return 0;

}

////////////6569////////////////////

uint8_t M6569_Regs[0x40];		// NB: Over allocated for now!

uint32_t	cTable[16]={

#if 0
	0x00000000,0x00ffffff,0x00782922,0x0087d6dd,0x00aa5fb6,0x0055a049,0x0040318d,0x00bfce72,
	0x00aa7449,0x00eab489,0x00b86962,0x00c7ffff,0x00eaf9f6,0x0094e089,0x008071cc,0x00ffffb2,
#else
0x00000000,
0x00FFFFFF,
0x0068372B,
0x0070A4B2,
0x006F3D86,
0x00588D43,
0x00352879,
0x00B8C76F,
0x006F4F25,
0x00433900,
0x009A6759,
0x00444444,
0x006C6C6C,
0x009AD284,
0x006C5EB5,
0x00959595
#endif
			};

uint32_t RASTER_CNT=0;
uint32_t RASTER_CMP=0;

uint8_t GetByte6569(int regNo)
{
	if (regNo<0x2F)
	{
		return M6569_Regs[regNo];
	}
	return 0xFF;
}

void SetByte6569(int regNo,uint8_t byte)
{
	if (regNo<0x2F)
	{
		if (regNo==0x12)
		{
			RASTER_CMP&=0x100;
			RASTER_CMP|=byte;
			return;
		}
		if (regNo==0x11)
		{
			RASTER_CMP&=0xFF;
			RASTER_CMP|=(byte&0x80)<<1;
			byte&=0x7F;
			byte|=M6569_Regs[0x11]&0x80;
		}
		if (regNo==0x19)
		{
			byte&=0x7F;
			byte=M6569_Regs[regNo]&(~byte);
			if ((byte&0xF)==0)
				byte&=0x7F;
		}
		M6569_Regs[regNo]=byte;
	}
}


#if 0
int16_t	channel1Level=0;
int16_t channel2Level=0;
int16_t	channel3Level=0;
int16_t channel4Level=0;
#endif

uint8_t GetByteFrom6569(uint16_t addr)
{
	uint16_t bank;
	uint8_t map;

	// Need to or in two bits from CIA2 to make up bits 15-14 of address
	bank=((~CIA1_PinGetPIN_PA())&0x3)<<14;

	addr&=0x3FFF;
	addr|=bank;

	map=addr>>12;

	return v_memMap[map](addr);
}


//Cycl-# 6                   1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3 3 3 3 3 3 3 3 3 4 4 4 4 4 4 4 4 4 4 5 5 5 5 5 5 5 5 5 5 6 6 6 6
//       3 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 1
//        _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
//    ø0 _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
//       __
//   IRQ   ________________________________________________________________________________________________________________________________
//       ________________________                                                                                      ____________________
//    BA                         ______________________________________________________________________________________
//        _ _ _ _ _ _ _ _ _ _ _ _ _ _ _                                                                                 _ _ _ _ _ _ _ _ _ _
//   AEC _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _________________________________________________________________________________ _ _ _ _ _ _ _ _ _
//
//   VIC i 3 i 4 i 5 i 6 i 7 i r r r r rcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcgcg i i 0 i 1 i 2 i 3
//  6510  x x x x x x x x x x x x X X X                                                                                 x x x x x x x x x x
//
//Graph.                      |===========01020304050607080910111213141516171819202122232425262728293031323334353637383940=========
//
//X coo. \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//       1111111111111111111111111110000000000000000000000000000000000000000000000000000000000000000111111111111111111111111111111111111111
//       89999aaaabbbbccccddddeeeeff0000111122223333444455556666777788889999aaaabbbbccccddddeeeeffff000011112222333344445555666677778888999
//       c048c048c048c048c048c048c04048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048c048

uint8_t mBorder=1;
uint8_t vBorder=1;
uint16_t xCnt=0x190;
uint16_t VCBase=0;
uint16_t VC=0;
uint8_t	RC=0;

uint8_t bufccode;
uint8_t bufccode1;
uint8_t bufccol;
uint8_t bufccol1;
uint8_t bufpixels;
uint8_t bufpixels1;
uint8_t ccode;
uint8_t ccol;
uint8_t pixels;

uint16_t xlCmp=0x18;
uint16_t xrCmp=0x158;
uint8_t  ytCmp=0x33;
uint8_t  ybCmp=0xFB;

uint32_t spData[8];
uint16_t MCBase[8];
uint16_t MC[8];
uint8_t  spDma[8]={0,0,0,0,0,0,0,0};

void Tick6569_OnePix();

void Tick6569()		// Needs to do 8 pixels - or 4 if ticked properly - ie once per half cpu clock
{
	int a,b;

	if (xCnt==0)
	{
		VC=VCBase;
		if (/*RASTER_CNT>=0x30 && RASTER_CNT<=0xF7 &&*/ (RASTER_CNT&7)==(M6569_Regs[0x11]&0x7))
		{
			RC=0;
		}
	}
	M6569_IRQ=1;

	if ((M6569_Regs[0x19] & M6569_Regs[0x1A])&0x01)
	{
		M6569_IRQ=0;
	}

	if ((xCnt&4)==0)
	{
		bufccode=bufccode1;
		bufccol=bufccol1;
		bufpixels=bufpixels1;
	}
	if ((xCnt&4)==0 && xCnt>=0x010 && xCnt<=0x148)
	{
		// Read C (note that this is wrong.. it should only be done on a BAD LINE)
		uint16_t screenAddress = (M6569_Regs[0x18]&0xF0)<<6;

		bufccode1 = GetByteFrom6569(screenAddress+(VC&0x3FF));
		bufccol1 = CRam[VC&0x3FF]&0x0F;
	}
	if ((xCnt&4)==4 && xCnt>=0x014 && xCnt<=0x14C)
	{
		uint16_t charAddress;
	       	
		if (M6569_Regs[0x11]&0x20)
		{
			charAddress = (M6569_Regs[0x18]&0x08)<<10;
			if (M6569_Regs[0x11]&0x40)				//TODO
			{
				bufpixels1=GetByteFrom6569(charAddress+((VC&0x3FF)*8)+RC);
			}
			else
			{
				bufpixels1=GetByteFrom6569(charAddress+((VC&0x3FF)*8)+RC);
			}
		}
		else
		{
			charAddress = (M6569_Regs[0x18]&0x0E)<<10;
			if (M6569_Regs[0x11]&0x40)
			{
				bufpixels1=GetByteFrom6569(charAddress+(bufccode1&0x1F)*8 + RC);
			}
			else
			{
				bufpixels1=GetByteFrom6569(charAddress+bufccode1*8 + RC);
			}
		}

		VC++;
	}
	if (xCnt==0x160)
	{
		if (RC==7)
		{
			VCBase=VC;
		}
		if (RASTER_CNT>=0x30 && RASTER_CNT<=0xF9 /*&& (RASTER_CNT&7)==(M6569_Regs[0x11]&0x7)*/)
		{
			RC++;
			RC&=7;
		}
	}
	if (xCnt==xrCmp)		// TODO fix for CSEL bit
	{
		mBorder=1;
	}
	if ((xCnt==0x18C))
	{
		RASTER_CNT++;
		if (RASTER_CNT>=312)
		{
			RASTER_CNT=0;
			VCBase=0;
		}
		M6569_Regs[0x11]&=0x7F;
		M6569_Regs[0x11]|=(RASTER_CNT>>1)&0x80;
		M6569_Regs[0x12]=RASTER_CNT&0xFF;
	
		if (RASTER_CMP==RASTER_CNT)
		{
			M6569_Regs[0x19]=0x81;	// IRQ + RASTER
		}

		if (RASTER_CNT==ybCmp)
		{
			vBorder=1;
		}
		if ((RASTER_CNT==ytCmp) && (M6569_Regs[0x11]&0x10))
		{
			vBorder=0;
		}
	}
	if ((xCnt==xlCmp) && (RASTER_CNT==ybCmp))
	{
		vBorder=1;
	}
	if ((xCnt==xlCmp) && (RASTER_CNT==ytCmp) && (M6569_Regs[0x11]&0x10))
	{
		vBorder=0;
	}
	if ((xCnt==xlCmp) && (vBorder==0))
	{
		mBorder=0;
	}

	if (xCnt>=0x164 && xCnt<=0x1E0)
	{
		int spNum;
		int spPhase;

		spNum=(xCnt-0x164)/16;
		spPhase=((xCnt-0x164)/4)&3;

		uint8_t spriteY=M6569_Regs[0x01+spNum*2];

		switch (spPhase)
		{
			case 0:			// Fetch address for sprite pixels - only done if spriteposy == raster_cnt
				if (RASTER_CNT==spriteY)
				{
					uint16_t screenAddress = (M6569_Regs[0x18]&0xF0)<<6;
					uint16_t screenOffs = 1024-(8-spNum);// (((RASTER_CNT-(51+yScr))/8) * 40) + (xCnt*8-(24+xScr))/8;
					
					MCBase[spNum]=GetByteFrom6569(screenAddress+screenOffs)*64;
					spDma[spNum]=1;
					MC[spNum]=0;
				}
				break;
			case 1:
				if (spDma[spNum])
				{
					spData[spNum]&=0x00FFFF;
					spData[spNum]|=GetByteFrom6569(MCBase[spNum]+MC[spNum])<<16;
					MC[spNum]++;
				}
				break;
			case 2:
				if (spDma[spNum])
				{
					spData[spNum]&=0xFF00FF;
					spData[spNum]|=GetByteFrom6569(MCBase[spNum]+MC[spNum])<<8;
					MC[spNum]++;
				}
				break;
			case 3:
				if (spDma[spNum])
				{
					spData[spNum]&=0xFFFF00;
					spData[spNum]|=GetByteFrom6569(MCBase[spNum]+MC[spNum]);
					MC[spNum]++;
					if (MC[spNum]==63)
					{
						spDma[spNum]=0;
					}
				}
				break;
		}
	}

	///////
	for (b=0;b<4;b++)
	{
		if ((xCnt&7)==(M6569_Regs[0x16]&7))
		{
			ccode=bufccode;
			ccol=bufccol;
			pixels=bufpixels;
		}
		Tick6569_OnePix();
		xCnt++;
	}
	///////
	if (xCnt==0x1F8)
	{
		xCnt=0;
	}

}

void Tick6569_OnePix()
{
	uint32_t* outputTexture = (uint32_t*)videoMemory[MAIN_WINDOW];
	uint32_t borderCol=cTable[M6569_Regs[0x20]&0xF];
	uint32_t inkCol1;
	uint32_t inkCol2;
	uint32_t inkCol;
	int MC=0;
	uint32_t paperCol=cTable[M6569_Regs[0x21]&0xF];
	int a;


	if (M6569_Regs[0x16]&0x10)
	{
		if (M6569_Regs[0x11]&0x20)
		{
			MC=1;
			inkCol=cTable[ccol&0xF];
			inkCol1=cTable[(ccode&0xF0)>>4];
			inkCol2=cTable[ccode&0xF];
		}
		else
		{
			MC=ccol&0x08;
			inkCol=cTable[ccol&0x7];
			inkCol1=cTable[M6569_Regs[0x22]&0xF];
			inkCol2=cTable[M6569_Regs[0x23]&0xF];
		}
	}
	else
	{
		if (M6569_Regs[0x11]&0x20)
		{
			paperCol=cTable[ccode&0xF];
			inkCol=cTable[(ccode&0xF0)>>4];
		}
		else
		{
			if (M6569_Regs[0x11]&0x40)
			{
				paperCol=cTable[M6569_Regs[0x21+((ccode&0xC0)>>6)]&0xF];
			}
			inkCol=cTable[ccol];
		}
	}

	if (mBorder || vBorder)
	{
		outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=borderCol;
	}
	else
	{
		if (MC)
		{
			switch(pixels&0xC0)
			{
				case 0x00:
					outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=paperCol;
					break;
				case 0x40:
					outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=inkCol1;
					break;
				case 0x80:
					outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=inkCol2;
					break;
				case 0xC0:
					outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=inkCol;
					break;
			}

			if ((xCnt+(M6569_Regs[0x11]&7))&1)
			{
				pixels<<=2;
			}
		}
		else
		{
			if (pixels&0x80)
			{
				outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=inkCol;
			}
			else
			{
				outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=paperCol;
			}
			pixels<<=1;
		}

		for (a=0;a<8;a++)
		{
			uint16_t spriteX=M6569_Regs[0x00+a*2];

			if (M6569_Regs[0x10]&(1<<a))
			{
				spriteX|=0x100;
			}
			if (xCnt>=spriteX && M6569_Regs[0x15]&(1<<a))
			{
				if (M6569_Regs[0x1C]&(1<<a))
				{
					// Multi-Colour

					switch(spData[a]&0xC00000)
					{
						case 0x000000:
							break;
						case 0x400000:
							outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=cTable[M6569_Regs[0x25]&0x0F];
							break;
						case 0x800000:
							outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=cTable[M6569_Regs[0x27+a]&0x0F];
							break;
						case 0xC00000:
							outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=cTable[M6569_Regs[0x26]&0x0F];
							break;
					}

					if ((spriteX-xCnt)&1)
					{
						spData[a]&=0x3FFFFF;
						spData[a]<<=2;
					}
				}
				else
				{
					if (spData[a]&0x800000)
					{
						outputTexture[xCnt + ((RASTER_CNT)*WIDTH)]=cTable[M6569_Regs[0x27+a]&0x0F];
					}
					spData[a]&=0x7FFFFF;
					spData[a]<<=1;
				}
			}	
		}

	}
}

//////////////////////// NOISES //////////////////////////

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

#define NUM_SRC_BUFFERS		2
#define BUFFER_LEN		(44100/50)

BUFFER_FORMAT audioBuffer[NUM_SRC_BUFFERS][BUFFER_LEN];
int audioBufferFull[NUM_SRC_BUFFERS];
int numBufferFull=0;
ALuint bufferFree[NUMBUFFERS];
int numBufferFree=0;

int curAudioBuffer=0;
int firstUsedBuffer=0;
int amountAdded=0;

void AudioInitialise()
{
	int a=0,b;
	for (b=0;b<NUM_SRC_BUFFERS;b++)
	{
		for (a=0;a<BUFFER_LEN;a++)
		{
			audioBuffer[b][a]=0;
		}
		audioBufferFull[b]=NUM_SRC_BUFFERS;
	}

	ALFWInitOpenAL();

	/* Generate some AL Buffers for streaming */
	alGenBuffers( NUMBUFFERS, uiBuffers );

	/* Generate a Source to playback the Buffers */
	alGenSources( 1, &uiSource );

	for (a=0;a<NUMBUFFERS;a++)
	{
		alBufferData(uiBuffers[a], AL_FORMAT, audioBuffer[0], BUFFER_LEN*BUFFER_FORMAT_SIZE, 44100);
		alSourceQueueBuffers(uiSource, 1, &uiBuffers[a]);
	}

	alSourcePlay(uiSource);
}


void AudioKill()
{
	ALFWShutdownOpenAL();
}

int16_t currentDAC[4] = {0,0,0,0};

void _AudioAddData(int channel,int16_t dacValue)
{
	currentDAC[channel]=dacValue;
}

uint32_t tickCnt=0;
uint32_t tickRate=((63*312*4096)/(44100/50));

/* audio ticked at same clock as everything else... so need a step down */
void UpdateAudio()
{
	tickCnt+=1*4096;
	
	if (tickCnt>=tickRate*50)
	{
		tickCnt-=tickRate;

		if (amountAdded==BUFFER_LEN)
		{
			audioBufferFull[numBufferFull]=curAudioBuffer;
			numBufferFull++;
			curAudioBuffer++;
			curAudioBuffer%=NUM_SRC_BUFFERS;
			if (numBufferFull==NUM_SRC_BUFFERS)
			{
				printf("Src Buffer Overrun - \n");
				numBufferFull--;
			}
			amountAdded=0;
		}
		int32_t res=0;
#if 1
		res+=currentDAC[0];
		res+=currentDAC[1];
		res+=currentDAC[2];
		res+=currentDAC[3];
#endif
//		res+=casLevel?1024:-1024;
/*
		static uint16_t bob=0;

		res=bob&0x8000?bob^0xFFFF:bob;

		bob+=1024;
*/
		audioBuffer[curAudioBuffer][amountAdded]=res>>BUFFER_FORMAT_SHIFT;
		amountAdded++;
	}

	if (numBufferFull)
	{
		int a;
		ALint processed;
		ALint state;
		alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &processed);

		while (processed>0)
		{
			ALuint buffer;

			amountAdded=0;
			alSourceUnqueueBuffers(uiSource,1, &bufferFree[numBufferFree++]);
			processed--;
		}

		while (numBufferFree && numBufferFull)
		{
			alBufferData(bufferFree[--numBufferFree], AL_FORMAT, audioBuffer[audioBufferFull[0]], BUFFER_LEN*BUFFER_FORMAT_SIZE, 44100);
			alSourceQueueBuffers(uiSource, 1, &bufferFree[numBufferFree]);
			for (a=1;a<numBufferFull;a++)
			{
				audioBufferFull[a-1]=audioBufferFull[a];
			}
			numBufferFull--;
		}

		alGetSourcei(uiSource, AL_SOURCE_STATE, &state);
		if (state!=AL_PLAYING)
		{
			alSourcePlay(uiSource);
		}
	}
}


///// TAP SUPPORT //////

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

int LoadTAP(const char* fileName)
{
	uint32_t tapeLength;
	uint32_t length;
	uint32_t tapePos;
	uint8_t *tapeBuffer;

	tapeBuffer=qLoad(fileName,&length);
	if (tapeBuffer==NULL)
	{
		printf("Failed to load %s\n",fileName);
		return 0;
	}

	if (strncmp(tapeBuffer,"C64-TAPE-RAW",12)!=0)
	{
		printf("Not a RAW TAP file\n");
		free(tapeBuffer);
		return 0;
	}

	if (tapeBuffer[12]!=1)
	{
		printf("Version Mismatch\n");
		free(tapeBuffer);
		return 0;
	}

	tapeLength=tapeBuffer[16];
	tapeLength|=tapeBuffer[17]<<8;
	tapeLength|=tapeBuffer[18]<<16;
	tapeLength|=tapeBuffer[19]<<24;

	if (tapeLength!=length-0x14)
	{
		printf("Tape Length Mismatch\n");
		printf("Correcting");
		tapeLength=length-0x14;
//		free(tapeBuffer);
//		return 0;
	}

	tapePos=0;
	cntPos=0;
	while (tapePos<tapeLength)
	{
		if (tapeBuffer[0x14+tapePos]==0)
		{
			cntBuffer[cntPos]=tapeBuffer[0x14+tapePos+1];
			cntBuffer[cntPos]|=tapeBuffer[0x14+tapePos+2]<<8;
			cntBuffer[cntPos]|=tapeBuffer[0x14+tapePos+3]<<16;
			cntBuffer[cntPos]>>=1;
			cntPos++;
			cntBuffer[cntPos]=cntBuffer[cntPos-1];
			tapePos+=4;
		}
		else
		{
			cntBuffer[cntPos]=tapeBuffer[0x14+tapePos]*4;
			cntPos++;
			cntBuffer[cntPos]=tapeBuffer[0x14+tapePos]*4;
			tapePos++;
		}

		cntPos++;
		if (cntPos>INTERNAL_TAPE_BUFFER_SIZE)
		{
			printf("Internal Tape Buffer Overrun\n");
			free(tapeBuffer);
			return 0;
		}
	}
	
	free(tapeBuffer);

	cntMax=cntPos;
	cntPos=0;

	return 1;
}

void SaveTAP(const char* filename)
{
	uint32_t tapeLength;
	uint32_t length;
	uint32_t tapePos;
	uint8_t *tapeBuffer;

	tapeBuffer=malloc(0x14);//qLoad(fileName,&length);
	if (tapeBuffer==NULL)
	{
		printf("Failed to save %s\n",filename);
		return;
	}

	memcpy(tapeBuffer,"C64-TAPE-RAW",12);
	tapeBuffer[12]=1;

	tapeLength=0;
	cntPos=0;
	while (cntPos<cntMax)
	{
		length=cntBuffer[cntPos++];
		length+=cntBuffer[cntPos++];
		length/=8;
		if (length>255)
		{
			tapeLength+=4;
		}
		else
		{
			tapeLength+=1;
		}
	}

	tapeBuffer[16]=tapeLength&0xFF;
	tapeBuffer[17]=(tapeLength>>8)&0xFF;
	tapeBuffer[18]=(tapeLength>>16)&0xFF;
	tapeBuffer[19]=(tapeLength>>24)&0xFF;
	
	//WRITE OUT HEADER
	FILE *out;

	out = fopen(filename,"wb");
	if (!out)
	{
		return;
	}

	fwrite(tapeBuffer,1,0x14,out);
	
	cntPos=0;
	while (cntPos<cntMax)
	{
		length=cntBuffer[cntPos++];
		length+=cntBuffer[cntPos++];
		length/=8;
		if (length>255)
		{
			tapeBuffer[0]=0;
			tapeBuffer[1]=length&0xFF;
			tapeBuffer[2]=(length>>8)&0xFF;
			tapeBuffer[3]=(length>>16)&0xFF;
			fwrite(tapeBuffer,1,4,out);
		}
		else
		{
			tapeBuffer[0]=length&0xFF;
			fwrite(tapeBuffer,1,1,out);
		}
	}

	fclose(out);

	cntPos=0;


}

uint8_t bytechecksum=0;

void OutputByteCheckMarker()
{
	bytechecksum=0;
}

void OutputBytePulses(uint8_t byte)
{
	uint8_t bitMask=0x01;
	int a;
	int even=1;

	bytechecksum^=byte;

	for (a=0;a<8;a++)
	{
		if (byte&bitMask)
		{
			cntBuffer[cntPos++]=0x3f*4;
			cntBuffer[cntPos++]=0x3f*4;
			cntBuffer[cntPos++]=0x2b*4;
			cntBuffer[cntPos++]=0x2b*4;
			even^=1;
		}
		else
		{
			cntBuffer[cntPos++]=0x2b*4;
			cntBuffer[cntPos++]=0x2b*4;
			cntBuffer[cntPos++]=0x3f*4;
			cntBuffer[cntPos++]=0x3f*4;
			even^=0;
		}
		bitMask<<=1;
	}

	if (even)
	{
		cntBuffer[cntPos++]=0x3f*4;
		cntBuffer[cntPos++]=0x3f*4;
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x2b*4;
	}
	else
	{
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x3f*4;
		cntBuffer[cntPos++]=0x3f*4;
	}
			
	cntBuffer[cntPos++]=0x53*4;
	cntBuffer[cntPos++]=0x53*4;
	cntBuffer[cntPos++]=0x3f*4;
	cntBuffer[cntPos++]=0x3f*4;
}

void OutputWordPulses(uint16_t word)
{
	OutputBytePulses(word&0xFF);
	OutputBytePulses(word>>8);
}

void OutputCheckPulses()
{
	OutputBytePulses(bytechecksum);
	cntPos-=4;
	cntBuffer[cntPos++]=0x53*4;
	cntBuffer[cntPos++]=0x53*4;
	cntBuffer[cntPos++]=0x2b*4;
	cntBuffer[cntPos++]=0x2b*4;
}

void OutputHeader(uint16_t startAddress,uint16_t length)
{
	uint32_t tapePos;

// HEADER

	for (tapePos=0;tapePos<0x6A00;tapePos++)		//PILOT
	{
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x2b*4;
	}

	cntBuffer[cntPos++]=0x53*4;				//NEW DATA MARKER
	cntBuffer[cntPos++]=0x53*4;				//NEW DATA MARKER
	cntBuffer[cntPos++]=0x3f*4;
	cntBuffer[cntPos++]=0x3f*4;
	
	OutputBytePulses(0x89);
	OutputBytePulses(0x88);
	OutputBytePulses(0x87);
	OutputBytePulses(0x86);
	OutputBytePulses(0x85);
	OutputBytePulses(0x84);
	OutputBytePulses(0x83);
	OutputBytePulses(0x82);
	OutputBytePulses(0x81);

	OutputByteCheckMarker();

	OutputBytePulses(0x03);		// PRG BLOCK

	OutputWordPulses(startAddress);			// Start Address
	OutputWordPulses(startAddress+(length-2));	// End Address

	OutputBytePulses('H');		// Filename
	OutputBytePulses('E');
	OutputBytePulses('L');
	OutputBytePulses('L');
	OutputBytePulses('O');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');

	for (tapePos=0;tapePos<171;tapePos++)
	{
		OutputBytePulses(' ');
	}

	OutputCheckPulses();

// HEADER REPEATED

	for (tapePos=0;tapePos<0x4F;tapePos++)			//PILOT
	{
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x2b*4;
	}

	cntBuffer[cntPos++]=0x53*4;				//NEW DATA MARKER
	cntBuffer[cntPos++]=0x53*4;				//NEW DATA MARKER
	cntBuffer[cntPos++]=0x3f*4;
	cntBuffer[cntPos++]=0x3f*4;
	
	OutputBytePulses(0x09);
	OutputBytePulses(0x08);
	OutputBytePulses(0x07);
	OutputBytePulses(0x06);
	OutputBytePulses(0x05);
	OutputBytePulses(0x04);
	OutputBytePulses(0x03);
	OutputBytePulses(0x02);
	OutputBytePulses(0x01);

	OutputByteCheckMarker();

	OutputBytePulses(0x03);		// PRG BLOCK

	OutputWordPulses(startAddress);			// Start Address
	OutputWordPulses(startAddress+(length-2));	// End Address

	OutputBytePulses('H');		// Filename
	OutputBytePulses('E');
	OutputBytePulses('L');
	OutputBytePulses('L');
	OutputBytePulses('O');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');
	OutputBytePulses(' ');

	for (tapePos=0;tapePos<171;tapePos++)
	{
		OutputBytePulses(' ');
	}

	OutputCheckPulses();
	
	for (tapePos=0;tapePos<0x4e;tapePos++)			//TRAILER
	{
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x2b*4;
	}
}

void OutputSilence()
{
	cntBuffer[cntPos++]=0x4000;
	cntBuffer[cntPos++]=0x4000;
}

void OutputData(uint8_t* tapeBuffer,uint16_t length)
{
	uint32_t tapePos;

	// DATA
	
	for (tapePos=0;tapePos<0x1A00;tapePos++)		//PILOT
	{
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x2b*4;
	}

	cntBuffer[cntPos++]=0x53*4;				//NEW DATA MARKER
	cntBuffer[cntPos++]=0x53*4;				//NEW DATA MARKER
	cntBuffer[cntPos++]=0x3f*4;
	cntBuffer[cntPos++]=0x3f*4;
	
	OutputBytePulses(0x89);
	OutputBytePulses(0x88);
	OutputBytePulses(0x87);
	OutputBytePulses(0x86);
	OutputBytePulses(0x85);
	OutputBytePulses(0x84);
	OutputBytePulses(0x83);
	OutputBytePulses(0x82);
	OutputBytePulses(0x81);

	OutputByteCheckMarker();

	for (tapePos=0;tapePos<length;tapePos++)
	{
		OutputBytePulses(tapeBuffer[tapePos]);
	}
	
	OutputCheckPulses();

// DATA REPEAT
	
	for (tapePos=0;tapePos<0x4f;tapePos++)		//PILOT
	{
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x2b*4;
	}

	cntBuffer[cntPos++]=0x53*4;				//NEW DATA MARKER
	cntBuffer[cntPos++]=0x53*4;				//NEW DATA MARKER
	cntBuffer[cntPos++]=0x3f*4;
	cntBuffer[cntPos++]=0x3f*4;
	
	OutputBytePulses(0x09);
	OutputBytePulses(0x08);
	OutputBytePulses(0x07);
	OutputBytePulses(0x06);
	OutputBytePulses(0x05);
	OutputBytePulses(0x04);
	OutputBytePulses(0x03);
	OutputBytePulses(0x02);
	OutputBytePulses(0x01);

	OutputByteCheckMarker();

	for (tapePos=0;tapePos<length;tapePos++)
	{
		OutputBytePulses(tapeBuffer[tapePos]);
	}
	
	OutputCheckPulses();
	
	for (tapePos=0;tapePos<0x4e;tapePos++)			//TRAILER
	{
		cntBuffer[cntPos++]=0x2b*4;
		cntBuffer[cntPos++]=0x2b*4;
	}
}

int LoadPRG(const char* fileName)
{
	uint16_t startAddress;
	uint32_t tapeLength;
	uint32_t length;
	uint32_t tapePos;
	uint8_t *tapeBuffer;

	tapeBuffer=qLoad(fileName,&length);
	if (tapeBuffer==NULL)
	{
		printf("Failed to load %s\n",fileName);
		return 0;
	}

	startAddress=tapeBuffer[0];
	startAddress|=tapeBuffer[1]<<8;

	cntPos=0;
	OutputHeader(startAddress,length);
	OutputSilence();
	OutputData(tapeBuffer+2,length-2);
	OutputSilence();

	free(tapeBuffer);

	cntMax=cntPos;
	cntPos=0;

	return 1;
}

int LoadCRT(const char* fileName)
{
	uint16_t type;
	uint32_t headerLength;
	uint32_t chipLength;
	uint16_t chipType;
	uint16_t bankNum;
	uint16_t loadAddr;
	uint16_t romSize;
	uint32_t length;
	uint32_t pos;
	uint8_t *crtBuffer;

	crtBuffer=qLoad(fileName,&length);
	if (crtBuffer==NULL)
	{
		printf("Failed to load %s\n",fileName);
		return 0;
	}

	// Parse Header
	
	if (strncmp(crtBuffer,"C64 CARTRIDGE   ",16)!=0)
	{
		printf("Not a CRT file\n");
		free(crtBuffer);
		return 0;
	}

	if (crtBuffer[0x14]!=1 || crtBuffer[0x15]!=0)
	{
		printf("Unsupported CRT version\n");
		free(crtBuffer);
		return 0;
	}

	headerLength =crtBuffer[0x10]<<24;
	headerLength|=crtBuffer[0x11]<<16;
	headerLength|=crtBuffer[0x12]<<8;
	headerLength|=crtBuffer[0x13];

	if (headerLength!=0x40)
	{
		printf("Unsupported CRT Header Length\n");
		free(crtBuffer);
		return 0;
	}

	type =crtBuffer[0x16]<<8;
	type|=crtBuffer[0x17];

	if (type!=0)
	{
		printf("Unsupported CRT Type (%02X)\n",type);
		free(crtBuffer);
		return 0;
	}

	PLA_EXROM=crtBuffer[0x18]&1;
	PLA_GAME=crtBuffer[0x19]&1;

	printf("CRT EXROM : %d\n",crtBuffer[0x18]);
	printf("CRT GAME : %d\n",crtBuffer[0x19]);

	// Jump to 0x40 and start looking at the CHIP data
	
	pos=0x40;
	while (1)
	{
		if (strncmp(&crtBuffer[pos],"CHIP",4)!=0)
		{
			printf("Seemingly corrupt cartridge\n");
			free(crtBuffer);
			return 0;
		}

		pos+=4;

		chipLength =crtBuffer[pos++]<<24;
		chipLength|=crtBuffer[pos++]<<16;
		chipLength|=crtBuffer[pos++]<<8;
		chipLength|=crtBuffer[pos++];

		chipType =crtBuffer[pos++]<<8;
		chipType|=crtBuffer[pos++];

		if (chipType!=0)
		{
			printf("Non ROM chips not supported\n");
			free(crtBuffer);
			return 0;
		}

		bankNum =crtBuffer[pos++]<<8;
		bankNum|=crtBuffer[pos++];

		if (bankNum!=0)
		{
			printf("Unsupported bank number\n");
			free(crtBuffer);
			return 0;
		}

		loadAddr =crtBuffer[pos++]<<8;
		loadAddr|=crtBuffer[pos++];

		romSize =crtBuffer[pos++]<<8;
		romSize|=crtBuffer[pos++];

		printf("CHIP : %04X (%04X)\n",loadAddr,romSize);

		testOverlay=&crtBuffer[pos];

		pos+=romSize;
		if (pos>=length)
			break;
	}

	ChangeMemoryMap();

	return 1;
}

void AttachImage(const char* fileName)
{
	cntPos=0;
	cntMax=0;

	if (LoadD64(fileName))
		return;
	if (LoadCRT(fileName))
		return;
	if (LoadTAP(fileName))
		return;
	if (LoadPRG(fileName))
		return;
}

//////////SID////////////

// voice |           24 bit Oscillator	->	Waveform Generator*4 (saw,triangle,pulse,random) (12bits out)	-> Waveform Selector -> D/A -> Multiplying D/A -> Envelope Generator (do this go here?)

uint8_t SID_6581_Regs[0x20];

uint32_t	oscillator[3];
uint32_t	noise[3]={0x7FFFF8,0x7FFFF8,0x7FFFF8};		// 23 bit lfsr
uint8_t		lastNoiseClock[3];	// 1 bit
uint8_t		envelopeCount[3];	// 8 bit
uint16_t	envelopeClock[3];	// 16 bit
uint8_t		envelopeState[3];	// 2 bit   0 - Release, 1 - attack, 2 - decay, 3 - sustain

uint16_t SID_FormSaw(int voice)
{
	return (oscillator[voice]&0xFFF000)>>12;
}

uint16_t SID_FormTriangle(int voice)
{
	uint32_t xor=(oscillator[voice]&0x800000)?0x7FF000:0x000000;

	return ((oscillator[voice]^xor)&0x7FF000)>>11;
}

uint16_t SID_FormPulse(int voice)
{
	uint16_t compare = SID_6581_Regs[0x02+voice*7];
	compare|=(SID_6581_Regs[0x03+voice*7]&0x0F)<<8;

	if (oscillator[voice]>compare)
		return 0x0FFF;
	return 0x0000;
}

uint16_t SID_FormRandom(int voice)
{
	uint16_t value;

	if (((oscillator[voice]>>19)^lastNoiseClock[voice])&1)
	{
		uint8_t tap1=(noise[voice]&(1<<22))>>22;
		uint8_t tap2=(noise[voice]&(1<<17))>>17;

		// clock noise shift register

		noise[voice]<<=1;
		noise[voice]|=tap1^tap2;
		noise[voice]&=0x7FFFFF;

		lastNoiseClock[voice]=(oscillator[voice]>>19)&1;
	}

	value = (noise[voice]&(1<<22))>>(22-7);
	value|= (noise[voice]&(1<<20))>>(20-6);
	value|= (noise[voice]&(1<<16))>>(16-5);
	value|= (noise[voice]&(1<<13))>>(13-4);
	value|= (noise[voice]&(1<<11))>>(11-3);
	value|= (noise[voice]&(1<<7))>>(7-2);
	value|= (noise[voice]&(1<<4))>>(4-1);
	value|= (noise[voice]&(1<<2))>>(2-0);

	return value<<4;
}

int SID_IsOscillating(int voice)
{
	uint16_t freq;
	freq=SID_6581_Regs[0x00 + voice*7];
	freq|=SID_6581_Regs[0x00 + voice*7]<<8;

	return freq!=0;
}

void SID_UpdateOscillator(int voice)
{
	uint16_t freq;
	freq=SID_6581_Regs[0x00 + voice*7];
	freq|=SID_6581_Regs[0x01 + voice*7]<<8;

	oscillator[voice]+=freq;
}

uint16_t envelopePeriod[16] = { 9, 32, 63, 95, 149, 220, 267, 313, 392, 977, 1954, 3126, 3907, 11720, 19532, 31251};

void SID_UpdateEnvelopeCount(int voice)
{
	uint8_t sustainLevel;

	sustainLevel=SID_6581_Regs[0x06 + voice*7]&0xF0;
	sustainLevel|=(SID_6581_Regs[0x06 + voice*7]&0xF0)>>4;

	switch (envelopeState[voice])
	{
		case 0:
			// Release
			if (envelopeCount[voice]!=0)
				envelopeCount[voice]--;
			break;
		case 1:	
			// Attack
			if (envelopeCount[voice]!=255)
				envelopeCount[voice]++;
			else
				envelopeState[voice]=2;
			break;
		case 2:
			// Decay
			if (envelopeCount[voice]!=sustainLevel)
				envelopeCount[voice]--;
			else
				envelopeState[voice]=3;
			break;
		case 3:
			// Sustain
			break;
	}
}

uint8_t SID_UpdateEnvelope(int voice)
{
	// update clock.. if timeover : 
	
	envelopeClock[voice]++;
	switch (envelopeState[voice])
	{
		case 0:
			if (envelopeClock[voice]>envelopePeriod[(SID_6581_Regs[0x06+voice*3]&0x0F)])
			{
				SID_UpdateEnvelopeCount(voice);
				envelopeClock[voice]=0;
			}
			break;
		case 1:
			if (envelopeClock[voice]>envelopePeriod[(SID_6581_Regs[0x05+voice*3]&0xF0)>>4])
			{
				SID_UpdateEnvelopeCount(voice);
				envelopeClock[voice]=0;
			}
			break;
		case 2:
			if (envelopeClock[voice]>envelopePeriod[(SID_6581_Regs[0x05+voice*3]&0x0F)*3])
			{
				SID_UpdateEnvelopeCount(voice);
				envelopeClock[voice]=0;
			}
			break;
	}
	return envelopeCount[voice];
}

void Reset6581()
{
	int a;
	for (a=0;a<3;a++)
	{
		oscillator[a]=0;
		noise[a]=0x7FFFF8;
		lastNoiseClock[a]=0;
		envelopeCount[a]=0;
		envelopeState[a]=0;
		envelopeClock[a]=0;
	}
}


void SetByte6581(uint16_t regNo,uint8_t byte)
{
	if (regNo==0x04 || regNo==(0x04+7) || regNo==(0x04+7*2))
	{
		// CTRL register write
		if (((SID_6581_Regs[regNo]&0x1)==0) && ((byte&1)==1))
		{
			// Start attack phase
			envelopeState[(regNo-4)/7]=1;
			envelopeClock[(regNo-4)/7]=0;

//			stopTheClock=1;
		}
		if (((SID_6581_Regs[regNo]&0x1)==1) && ((byte&1)==0))
		{
			// Start release phase
			envelopeState[(regNo-4)/7]=0;
			envelopeClock[(regNo-4)/7]=0;
		}
	}
	SID_6581_Regs[regNo]=byte;
}

void Tick6581()
{
	int a;

	for (a=0;a<3;a++)
	{
		uint32_t s,t,p,r;
		uint32_t e;


		s=SID_FormSaw(a);
		t=SID_FormTriangle(a);
		p=SID_FormPulse(a);
		r=SID_FormRandom(a);
		e=SID_UpdateEnvelope(a);

		int16_t	soundLevel=0;
		
		RecordPinA(a+3,e);
		if (SID_6581_Regs[0x04+a*7]&0x80)
		{
			RecordPinA(a,r>>4);
			soundLevel=0 + ((r*e)>>7);
		}
		if (SID_6581_Regs[0x04+a*7]&0x40)
		{
			RecordPinA(a,p>>4);
			soundLevel=0 + ((p*e)>>7);
		}
		if (SID_6581_Regs[0x04+a*7]&0x20)
		{
			RecordPinA(a,s>>4);
			soundLevel=0 + ((s*e)>>7);
		}
		if (SID_6581_Regs[0x04+a*7]&0x10)
		{
			RecordPinA(a,t>>4);
			soundLevel=0 + ((t*e)>>7);
		}
		
		if (soundLevel>8191)
		{
			printf("Clipping??? : %d\n",soundLevel);
		}
		RecordPinA(a+6,soundLevel>>5);

/*
		if ((SID_6581_Regs[0x04+a*7]&0x60)!=0 && soundLevel!=0)
		{
			printf("%d %04X %04X %04X %04X %02X %04X %s%s%s%s\n",a,s&0xFFF,t&0xFFF,p&0xFFF,r&0xFFF,e,soundLevel,
					SID_6581_Regs[0x04+a*7]&0x10 ? "T":" ",
					SID_6581_Regs[0x04+a*7]&0x20 ? "S":" ",
					SID_6581_Regs[0x04+a*7]&0x40 ? "P":" ",
					SID_6581_Regs[0x04+a*7]&0x80 ? "N":" "
					);
		}*/
/*
		if (soundLevel!=0)		// we wont see the release portion but this is a good start
		{
			printf("%d %04X %04X %04X %04X %02X %04X %s%s%s%s\n",a,s&0xFFF,t&0xFFF,p&0xFFF,r&0xFFF,e,soundLevel,
					SID_6581_Regs[0x04+a*7]&0x10 ? "T":" ",
					SID_6581_Regs[0x04+a*7]&0x20 ? "S":" ",
					SID_6581_Regs[0x04+a*7]&0x40 ? "P":" ",
					SID_6581_Regs[0x04+a*7]&0x80 ? "N":" "
					);
		}
*/
		SID_UpdateOscillator(a);

		//       implement filter

		_AudioAddData(a,soundLevel);
	}
}
