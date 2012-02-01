/*
 * NES implementation
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

#define IS_COMPAT		0

#define ENABLE_TV		1
#define ENABLE_LOGIC_ANALYSER	0
#define ENABLE_DEBUGGER		1

#include "jake\ntscDecode.h"

int edlVideoGeneration=1;

void AudioKill();
void AudioInitialise();
void UpdateAudio();
void _AudioAddData(int channel,int16_t dacValue);

extern uint8_t PPU_Control;
extern uint8_t PPU_Mask;
extern uint8_t PPU_Status;
extern uint16_t PPU_hClock;
extern uint16_t PPU_vClock;

void PPU_PinSetRW(uint8_t);
void PPU_PinSet_DBE(uint8_t);
void PPU_PinSetRS(uint8_t);
void PPU_PinSetD(uint8_t);
void PPU_PinSetCLK(uint8_t);
uint8_t PPU_PinGetD();
uint8_t PPU_PinGetPA();
uint8_t PPU_PinGetAD();
uint8_t PPU_PinGetALE();
uint8_t PPU_PinGet_RE();
uint8_t PPU_PinGet_WE();
uint8_t PPU_PinGetVIDEO();

uint16_t MAIN_PinGetA();
uint8_t MAIN_PinGetD();
void MAIN_PinSetD(uint8_t);
void MAIN_PinSetCLK(uint8_t);
uint8_t MAIN_PinGetRW();
void MAIN_PinSet_IRQ(uint8_t);
void MAIN_PinSet_RES(uint8_t);

uint8_t *prgRom;
uint32_t prgRomSize;
uint8_t *chrRom;
uint32_t chrRomSize;

uint8_t* bnk00chr;
uint8_t* bnk01chr;
uint8_t* bnk02chr;
uint8_t* bnk03chr;
uint8_t* bnk10chr;
uint8_t* bnk11chr;
uint8_t* bnk12chr;
uint8_t* bnk13chr;

uint8_t chrRam[8192];
uint8_t prgRam[8192];

uint8_t hMirror;
uint8_t vMirror;

int usingMMC1=0;
int usingMMC3=0;
int usingUxROM=0;
int usingCNROM=0;

// Step 1. Memory

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

void Tick2C02();

int InitialiseMemory()
{
	return 0;
}

uint8_t internalRam[0x800];
uint8_t ppuRam[0x800];

uint8_t PPUGetByte(uint16_t addr)
{
	addr&=0x3FFF;
	if (addr<0x400)
	{
		return bnk00chr[addr&0x03FF];
	}
	if (addr<0x800)
	{
		return bnk01chr[addr&0x03FF];
	}
	if (addr<0xC00)
	{
		return bnk02chr[addr&0x03FF];
	}
	if (addr<0x1000)
	{
		return bnk03chr[addr&0x03FF];
	}
	if (addr<0x1400)
	{
		return bnk10chr[addr&0x03FF];
	}
	if (addr<0x1800)
	{
		return bnk11chr[addr&0x03FF];
	}
	if (addr<0x1C00)
	{
		return bnk12chr[addr&0x03FF];
	}
	if (addr<0x2000)
	{
		return bnk13chr[addr&0x03FF];
	}
	uint16_t chrAddr=(addr-0x2000)&0x3FF;
	if (hMirror)
		chrAddr|=(addr&0x0800)>>1;
	if (vMirror)
		chrAddr|=(addr&0x0400);
	return ppuRam[chrAddr];		// WILL NEED FIXING
}
void PPUSetByte(uint16_t addr,uint8_t byte)
{
	addr&=0x3FFF;
	if (addr<0x2000)
	{
		chrRam[addr]=byte;
		return;		/// assuming fixed tile ram		<---- this might need to be pageable! but doubtful
	}
	uint16_t chrAddr=(addr-0x2000)&0x3FF;
	if (hMirror)
		chrAddr|=(addr&0x0800)>>1;
	if (vMirror)
		chrAddr|=(addr&0x0400);
	ppuRam[chrAddr]=byte;
}

int KeyDown(int key);
void ClearKey(int key);

uint8_t controllerData=0;

uint8_t* bnk0prg=NULL;
uint8_t* bnk1prg=NULL;
uint8_t* bnk2prg=NULL;
uint8_t* bnk3prg=NULL;

uint8_t GetByte(uint16_t addr)
{
	if (addr<0x2000)
	{
		return internalRam[addr&0x7FF];
	}
	if (addr<0x4000)
	{
		return;
	}
	if (addr<0x6000)
	{
		if (addr==0x4016)
		{
			uint8_t tmp=controllerData&0x80;
			controllerData<<=1;
			tmp>>=7;
			return tmp;
		}
		if (addr>=0x4017 && addr<=0x4017)	//controller ports
			return 0;
//		printf("Unkown Read : %08X\n",addr);
		return 0;
	}
	if (addr<0x8000)
	{
		return prgRam[addr-0x6000];
	}
	if (addr<0xA000)
	{
		return bnk0prg[addr-0x8000];
	}
	if (addr<0xC000)
	{
		return bnk1prg[addr-0xA000];
	}
	if (addr<0xE000)
	{
		return bnk2prg[addr-0xC000];
	}

	return bnk3prg[addr-0xE000];
}

uint8_t shiftRegister=0;
uint8_t shiftCount=0;

uint8_t MMC1_Control=0;
uint8_t MMC1_ChrBank0=0;
uint8_t MMC1_ChrBank1=0;
uint8_t MMC1_PrgBank=0;

void WriteMMC1_Control(uint8_t byte)	//NB need to set bank configs up here as well!!!
{
	byte&=0x1F;
	if (MMC1_Control!=byte)
		printf("MMC1 control new value %02X\n",byte);
	MMC1_Control=byte;
	switch (MMC1_Control&3)
	{
		case 0:
		case 1:
			hMirror=0;
			vMirror=0;
			break;
		case 2:
			hMirror=0;
			vMirror=1;
			break;
		case 3:
			hMirror=1;
			vMirror=0;
			break;
	}
}

void WriteMMC1_ChrBank0(uint8_t byte)
{
	byte&=0x1F;
	if (MMC1_ChrBank0!=byte)
		printf("MMC1 char0 bank %02X\n",byte);
	MMC1_ChrBank0=byte;
	if (MMC1_Control&0x10)
	{
		bnk00chr=&chrRom[0x1000*(byte&0x1F)];
		bnk01chr=&chrRom[0x1000*(byte&0x1F)+0x400];
		bnk02chr=&chrRom[0x1000*(byte&0x1F)+0x800];
		bnk03chr=&chrRom[0x1000*(byte&0x1F)+0xC00];
	}
	else
	{
		bnk00chr=&chrRom[0x2000*(byte&0x1E)];
		bnk01chr=&chrRom[0x2000*(byte&0x1E)+0x400];
		bnk02chr=&chrRom[0x2000*(byte&0x1E)+0x800];
		bnk03chr=&chrRom[0x2000*(byte&0x1E)+0xC00];
		bnk10chr=&chrRom[0x2000*(byte&0x1E)+0x1000];
		bnk11chr=&chrRom[0x2000*(byte&0x1E)+0x1400];
		bnk12chr=&chrRom[0x2000*(byte&0x1E)+0x1800];
		bnk13chr=&chrRom[0x2000*(byte&0x1E)+0x1C00];
	}
}

void WriteMMC1_ChrBank1(uint8_t byte)
{
	byte&=0x1F;
	if (MMC1_ChrBank1!=byte)
		printf("MMC1 char1 bank %02X\n",byte);
	MMC1_ChrBank1=byte;
	if (MMC1_Control&0x10)
	{
		bnk10chr=&chrRom[0x1000*(byte&0x1F)];
		bnk11chr=&chrRom[0x1000*(byte&0x1F)+0x400];
		bnk12chr=&chrRom[0x1000*(byte&0x1F)+0x800];
		bnk13chr=&chrRom[0x1000*(byte&0x1F)+0xC00];
	}
}

void WriteMMC1_PrgBank(uint8_t byte)
{
	//DBBBB  D-PRGRam Disable  - BBBB is bank
	byte&=0x1F;
	if (MMC1_PrgBank!=byte)
		printf("MMC1 prg bank %02X\n",byte);
	MMC1_PrgBank=byte;
	switch (MMC1_Control&0x0C)
	{
		case 0:
		case 0x4:
			bnk0prg=&prgRom[(byte&0xE)*32678];
			bnk1prg=&prgRom[(byte&0xE)*32678 + 8192];
			bnk2prg=&prgRom[(byte&0xE)*32678 + 16384];
			bnk3prg=&prgRom[(byte&0xE)*32678 + 16384 + 8192];
			break;
		case 0x8:
			bnk2prg=&prgRom[(byte&0xF)*16384];
			bnk3prg=&prgRom[(byte&0xF)*16384+8192];
			break;
		case 0xC:
			bnk0prg=&prgRom[(byte&0xF)*16384];
			bnk1prg=&prgRom[(byte&0xF)*16384+8192];
			break;
	}
}

void WriteToUxROM(uint16_t addr,uint8_t byte)
{
	byte&=0x7;
	bnk0prg=&prgRom[byte*16384];
	bnk1prg=&prgRom[byte*16384+8192];
}

void WriteToCNROM(uint16_t addr,uint8_t byte)
{
	byte&=0x3;
	bnk00chr=&chrRom[byte*0x2000];
	bnk01chr=&chrRom[byte*0x2000+0x400];
	bnk02chr=&chrRom[byte*0x2000+0x800];
	bnk03chr=&chrRom[byte*0x2000+0xC00];
	bnk10chr=&chrRom[byte*0x2000+0x1000];
	bnk11chr=&chrRom[byte*0x2000+0x1400];
	bnk12chr=&chrRom[byte*0x2000+0x1800];
	bnk13chr=&chrRom[byte*0x2000+0x1C00];
}


void WriteToMMC1(uint16_t addr,uint8_t byte)
{
	if (byte&0x80)
	{
		shiftRegister=0;
		shiftCount=0;
		WriteMMC1_Control(MMC1_Control|0x0C);
	}
	else
	{
		shiftRegister>>=1;
		shiftRegister&=0x0F;
		shiftRegister|=(byte&1)<<4;
		shiftCount++;
		if (shiftCount==5)
		{
			shiftCount=0;
			addr&=0x6000;
			addr>>=13;
			switch(addr)
			{
				case 0:
					WriteMMC1_Control(shiftRegister);
					break;
				case 1:
					WriteMMC1_ChrBank0(shiftRegister);
					break;
				case 2:
					WriteMMC1_ChrBank1(shiftRegister);
					break;
				case 3:
					WriteMMC1_PrgBank(shiftRegister);
					break;
			}
		}
	}

}

uint8_t MMC3_BankSel;
uint8_t MMC3_BankData[8]={0,0,0,0,0,0,0,0};

void WriteMMC3_SwitchPrg()
{
	if (MMC3_BankSel&0x40)
	{
		// -2|R7|R6|-1
		bnk0prg=&prgRom[(prgRomSize-16384)];
		bnk1prg=&prgRom[MMC3_BankData[7]*8192];
		bnk2prg=&prgRom[MMC3_BankData[6]*8192];
		bnk3prg=&prgRom[(prgRomSize-16384)+8192];
	}
	else
	{
		// R6|R7|-2|-1
		bnk0prg=&prgRom[MMC3_BankData[6]*8192];
		bnk1prg=&prgRom[MMC3_BankData[7]*8192];
		bnk2prg=&prgRom[(prgRomSize-16384)];
		bnk3prg=&prgRom[(prgRomSize-16384)+8192];
	}

}

void WriteMMC3_SwitchChr()
{
	if (MMC3_BankSel&0x80)
	{
		// R2|R3|R4|R5|R0|R0+|R1|R1+
		bnk00chr=&chrRom[MMC3_BankData[2]*1024];
		bnk01chr=&chrRom[MMC3_BankData[3]*1024];
		bnk02chr=&chrRom[MMC3_BankData[4]*1024];
		bnk03chr=&chrRom[MMC3_BankData[5]*1024];
		bnk10chr=&chrRom[MMC3_BankData[0]*2048];
		bnk11chr=&chrRom[MMC3_BankData[0]*2048+0x400];
		bnk12chr=&chrRom[MMC3_BankData[1]*2048];
		bnk13chr=&chrRom[MMC3_BankData[1]*2048+0x400];
	}
	else
	{
		// R0|R0+|R1|R1+|R2|R3|R4|R5
		bnk00chr=&chrRom[MMC3_BankData[0]*2048];
		bnk01chr=&chrRom[MMC3_BankData[0]*2048+0x400];
		bnk02chr=&chrRom[MMC3_BankData[1]*2048];
		bnk03chr=&chrRom[MMC3_BankData[1]*2048+0x400];
		bnk10chr=&chrRom[MMC3_BankData[2]*1024];
		bnk11chr=&chrRom[MMC3_BankData[3]*1024];
		bnk12chr=&chrRom[MMC3_BankData[4]*1024];
		bnk13chr=&chrRom[MMC3_BankData[5]*1024];
	}
}

void WriteMMC3_BankSelect(uint8_t byte)
{
	MMC3_BankSel=byte;

	WriteMMC3_SwitchPrg();
//	WriteMMC3_SwitchChr();
}

void WriteMMC3_BankData(uint8_t byte)
{
	MMC3_BankData[MMC3_BankSel&7]=byte;
	if ((MMC3_BankSel&7)<6)
	{
		//WriteMMC3_SwitchChr();
	}
	else
	{
		WriteMMC3_SwitchPrg();
	}
}

void WriteMMC3_Mirroring(uint8_t byte)
{
	if (byte&1)
	{
		hMirror=1;
		vMirror=0;
	}
	else
	{
		hMirror=0;
		vMirror=1;
	}
}


void WriteToMMC3(uint16_t addr,uint8_t byte)
{
	printf("MMC3 Write : %04X,%02X\n",addr,byte);
	addr&=0xE001;
	switch (addr)
	{
		case 0x8000:
			WriteMMC3_BankSelect(byte);
			break;
		case 0x8001:
			WriteMMC3_BankData(byte);
			break;
		case 0xA000:
			WriteMMC3_Mirroring(byte);
			break;
		default:
//			printf("MMC3 Write : %04X,%02X\n",addr,byte);
			break;
	}
}

void SetByte(uint16_t addr,uint8_t byte)
{
	if (addr<0x2000)
	{
		internalRam[addr&0x7FF]=byte;
		return;
	}
	if (addr<0x4000)
	{
		return;
	}
	if (addr<0x6000)
	{
		if (addr>=0x4014 && addr<=0x4017)		// sound channel + controller 2.. todo  - controller 1 will need rewriting when rp2a03 has proper register support
		{
			if (addr>=4016)
			{
				if (byte&1)
				{
					// Latch controller - reset shifter
					controllerData=0;
					if (KeyDown(GLFW_KEY_RIGHT))
					{
						controllerData|=0x01;
					}
					if (KeyDown(GLFW_KEY_LEFT))
					{
						controllerData|=0x02;
					}
					if (KeyDown(GLFW_KEY_DOWN))
					{
						controllerData|=0x04;
					}
					if (KeyDown(GLFW_KEY_UP))
					{
						controllerData|=0x08;
					}
					if (KeyDown(GLFW_KEY_ENTER))
					{
						controllerData|=0x10;
					}
					if (KeyDown(GLFW_KEY_SPACE))
					{
						controllerData|=0x20;
					}
					if (KeyDown('Z'))
					{
						controllerData|=0x40;
					}
					if (KeyDown('X'))
					{
						controllerData|=0x80;
					}
				}
			}
			return;
		}
//		printf("Unknown Write : %08X\n",addr);
		return;
	}
	if (addr<0x8000)
	{
		prgRam[addr-0x6000]=byte;
		return;
	}
	if (usingUxROM)
	{
		WriteToUxROM(addr,byte);
	}
	if (usingCNROM)
	{
		WriteToCNROM(addr,byte);
	}
	if (usingMMC1)
	{
		WriteToMMC1(addr,byte);
	}
	if (usingMMC3)
	{
		WriteToMMC3(addr,byte);
	}
	//printf("Unmapped Write : %08X\n",addr);
	return;
}

int masterClock=0;
int pixelClock=0;
int cpuClock=0;

extern uint8_t *MAIN_DIS_[256];

extern uint8_t	MAIN_A;
extern uint8_t	MAIN_X;
extern uint8_t	MAIN_Y;
extern uint16_t	MAIN_SP;
extern uint16_t	MAIN_PC;
extern uint8_t	MAIN_P;

extern uint8_t	MAIN_DEBUG_SYNC;

int stopTheClock=1;

uint32_t MAIN_missing(uint32_t opcode)
{
	printf("MAIN:Executing Missing Instruction!! : %04X:%02X\n",MAIN_PC-1,opcode);
	stopTheClock=1;
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

int g_instructionStep=0;
int g_traceStep=0;

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	(256+24)

#define HEIGHT	(262)
#define	WIDTH	(341)

#define NTSC_WIDTH	(NTSC_SAMPLES_PER_LINE)
#define NTSC_HEIGHT	(NTSC_LINES_PER_FRAME)

#define TIMING_WIDTH	640*2
#define TIMING_HEIGHT	320

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define NTSC_WINDOW		1
#define REGISTER_WINDOW		2
#define TIMING_WINDOW		3

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

int KeyDownWindow(int key,GLFWwindow window)
{
	return (keyArray[key].curState==GLFW_PRESS) && (keyArray[key].window==window);
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

void windowClearKey(int key)
{
	ClearKey(key);
}

int windowCheckKey(int key)
{
	return CheckKey(key);
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

void LoadCart(const char* fileName);

int dumpNTSC=0;

int MasterClock=0;

FILE* ntsc_file=NULL;

void AvgNTSC();				// kicks sample to ntsc tv (1/3 master clock rate)
void SampleNTSC(uint8_t videoLevel);	// adds sample to internal buffer to generate average for above

int NTSCClock;

static uint16_t lastPC;

void ClockCPU(int cpuClock)
{
	uint16_t addr; 
	//if (cpuClock==0)
	{
		MAIN_PinSetCLK(cpuClock&1);
#if ENABLE_LOGIC_ANALYSER
		RecordPin(0,cpuClock&1);
		RecordPin(1,MAIN_PinGetM2()&1);
#endif
		addr = MAIN_PinGetA();

		if (MAIN_DEBUG_SYNC)
		{
			lastPC=addr;

			if (isBreakpoint(0,lastPC))
			{
				stopTheClock=1;
			}
		}

		// Phase is 1 (LS139 decode gives us DBE low when Phase 1, Address=001x xxxx xxxx xxxx
		uint8_t dbe_signal=(MAIN_PinGetM2()&1) && ((addr&0x8000)==0) && ((addr&0x4000)==0) && ((addr&0x2000)==0x2000)?0:1;
#if ENABLE_LOGIC_ANALYSER
		RecordPin(2,dbe_signal&1);
#endif
		//_AudioAddData(0,(dbe_signal&1)?32767:-32768);

		{
			PPU_PinSetRS(addr&0x7);
			PPU_PinSetRW(MAIN_PinGetRW());
			PPU_PinSetD(MAIN_PinGetD());

			PPU_PinSet_DBE(dbe_signal);
			if (MAIN_PinGetRW())
			{
				MAIN_PinSetD(PPU_PinGetD());
			}
		}
		{
			// Because we are not yet handling ram/rom etc as chips - the below check takes care of making sure we only get a single access
			static int lastM2=1111;
			if (lastM2!=(MAIN_PinGetM2()&1))
			{
				lastM2=MAIN_PinGetM2()&1;
				if (lastM2==1 && dbe_signal)
				{
					if (MAIN_PinGetRW())
					{
						uint8_t  data = GetByte(addr);
						MAIN_PinSetD(data);
					}
					if (!MAIN_PinGetRW())
					{
						SetByte(addr,MAIN_PinGetD());
					}
				}
			}
		}

		MAIN_PinSet_IRQ(1);
		MAIN_PinSet_NMI(PPU_PinGet_INT());
	}

}

void ClockPPU(int ppuClock)
{
//	if (ppuClock==0)
	{
		static uint8_t vramLowAddressLatch=0;

		PPU_PinSetCLK(ppuClock&1);
		if (PPU_PinGetALE()&1)
		{
			vramLowAddressLatch=PPU_PinGetAD();
		}
		if ((PPU_PinGet_RE()&1)==0)
		{
			uint16_t addr = ((PPU_PinGetPA()&0x3F)<<8)|vramLowAddressLatch;
			PPU_PinSetAD(PPUGetByte(addr));
		}
		if ((PPU_PinGet_WE()&1)==0)
		{
			uint16_t addr = ((PPU_PinGetPA()&0x3F)<<8)|vramLowAddressLatch;
			PPUSetByte(addr,PPU_PinGetAD());
		}

	}

}

void GenerateNTSC(int colourClock);
void writeNTSC();

void TickChips(int MasterClock)
{
	int cpuClock=MasterClock%24;
	int ppuClock=MasterClock%8;
	int ColourClock=MasterClock%12;
	NTSCClock=MasterClock%3;

	ClockCPU(MasterClock);
	ClockPPU(MasterClock);
#if ENABLE_TV
//	if (ppuClock==0)
		Tick2C02();

	//			if (ntsc_file /*&& (NTSCClock==0)*/)
	{
		GenerateNTSC(ColourClock);
		if (NTSCClock==2)
			writeNTSC();
		SampleNTSC(PPU_PinGetVIDEO());
		if (NTSCClock==2)
		{
			AvgNTSC();
		}
	}
#endif
#if ENABLE_LOGIC_ANALYSER
	UpdatePinTick();
#endif
}

int main(int argc,char**argv)
{
	int w,h;
	double	atStart,now,remain;
	uint16_t bp;
	
	if (argc==2)
	{
		printf("Loading %s\n",argv[1]);
		LoadCart(argv[1]);
#if IS_COMPAT
		exit(0);
#endif
	}
	else
	{
		printf("Currently require a rom name as argument (iNes format)\n");
		exit(-1);
	}


	/// Initialize GLFW 
	glfwInit(); 

#if ENABLE_LOGIC_ANALYSER
	// Open timing OpenGL window 
	if( !(windows[TIMING_WINDOW]=glfwOpenWindow( TIMING_WIDTH, TIMING_HEIGHT, GLFW_WINDOWED,"Logic Analyser",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[TIMING_WINDOW],0,640);

	glfwMakeContextCurrent(windows[TIMING_WINDOW]);
	setupGL(TIMING_WINDOW,TIMING_WIDTH,TIMING_HEIGHT);
#endif

#if ENABLE_DEBUGGER
	// Open registers OpenGL window 
	if( !(windows[REGISTER_WINDOW]=glfwOpenWindow( REGISTER_WIDTH, REGISTER_HEIGHT, GLFW_WINDOWED,"Main CPU",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[REGISTER_WINDOW],300,0);

	glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
	setupGL(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);
#endif

#if ENABLE_TV
	if( !(windows[NTSC_WINDOW]=glfwOpenWindow( NTSC_WIDTH, NTSC_HEIGHT, GLFW_WINDOWED,"NTSC",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[NTSC_WINDOW],700,300);

	glfwMakeContextCurrent(windows[NTSC_WINDOW]);
	setupGL(NTSC_WINDOW,NTSC_WIDTH,NTSC_HEIGHT);
#endif
	// Open screen OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwOpenWindow( WIDTH, HEIGHT, GLFW_WINDOWED,"nes",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC

	glfwGetWindowSize(windows[MAIN_WINDOW],&w,&h);

	printf("width : %d (%d) , height : %d (%d)\n", w,WIDTH,h,HEIGHT);
	glfwSetKeyCallback(kbHandler);
	glfwSetWindowSizeCallback(sizeHandler);
	
#if ENABLE_TV
	ntscDecodeInit((uint32_t*)videoMemory[NTSC_WINDOW]);
#endif

	atStart=glfwGetTime();
	//////////////////
	AudioInitialise();

	if (InitialiseMemory())
		return -1;

	MAIN_PinSet_IRQ(1);
	MAIN_PinSet_NMI(1);

	MAIN_PC=GetByte(0xFFFC);
	MAIN_PC|=GetByte(0xFFFD)<<8;

//	MAIN_PC=0xc000;
// TODO PPU RESET + PROPER CPU RESET

	PPU_PinSet_RST(0);
	PPU_PinSet_RST(1);
	PPU_PinSet_DBE(1);

	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
	{
		
		if ((!stopTheClock) || g_traceStep || g_instructionStep)
		{
			// Tick Hardware based on MASTER clocks (or as close as damn it)


			TickChips(MasterClock);//cpuClock,ppuClock,ColourClock,NTSCClock);
			MasterClock+=1;
			UpdateAudio();
		}

		if (MasterClock>=341*262*2*4 || stopTheClock)
		{
			static int twofields=1;
			static int normalSpeed=1;

//			stopTheClock=1;
			if (MasterClock>=341*262*2*4)
			{
				twofields++;
				MasterClock-=341*262*2*4;
			}

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
 
#if ENABLE_TV			
			if (twofields&1)
			{
				ntscDecodeTick();
			}
			glfwMakeContextCurrent(windows[NTSC_WINDOW]);
			ShowScreen(NTSC_WINDOW,NTSC_WIDTH,NTSC_HEIGHT);
			glfwSwapBuffers();
#endif
	
#if ENABLE_DEBUGGER
			glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
			DrawRegisterMain(videoMemory[REGISTER_WINDOW],REGISTER_WIDTH,lastPC,GetByte);
			UpdateRegisterMain(windows[REGISTER_WINDOW]);
			ShowScreen(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);
			glfwSwapBuffers();
#endif

#if ENABLE_LOGIC_ANALYSER			
			glfwMakeContextCurrent(windows[TIMING_WINDOW]);
			DrawTiming(videoMemory[TIMING_WINDOW],TIMING_WIDTH);
			UpdateTimingWindow(windows[TIMING_WINDOW]);
			ShowScreen(TIMING_WINDOW,TIMING_WIDTH,TIMING_HEIGHT);
			glfwSwapBuffers();
#endif

			glfwPollEvents();
			
			if (CheckKey(GLFW_KEY_F1))
			{
				ClearKey(GLFW_KEY_F1);
				edlVideoGeneration^=1;
			}
			if (CheckKey(GLFW_KEY_INSERT))
			{
				ClearKey(GLFW_KEY_INSERT);
				normalSpeed^=1;
			}
			if (CheckKey(GLFW_KEY_DELETE))
			{
				ClearKey(GLFW_KEY_DELETE);
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
			if (CheckKey(GLFW_KEY_KP_ADD))
			{
				ClearKey(GLFW_KEY_KP_ADD);
				dumpNTSC=1;
			}
			
			now=glfwGetTime();

			remain = now-atStart;

			while ((remain<0.01666666f) && normalSpeed && !stopTheClock)
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

uint32_t nesColours[0x40]=
{
0x7C7C7C,
0x0000FC,
0x0000BC,
0x4428BC,
0x940084,
0xA80020,
0xA81000,
0x881400,
0x503000,
0x007800,
0x006800,
0x005800,
0x004058,
0x000000,
0x000000,
0x000000,
0xBCBCBC,
0x0078F8,
0x0058F8,
0x6844FC,
0xD800CC,
0xE40058,
0xF83800,
0xE45C10,
0xAC7C00,
0x00B800,
0x00A800,
0x00A844,
0x008888,
0x000000,
0x000000,
0x000000,
0xF8F8F8,
0x3CBCFC,
0x6888FC,
0x9878F8,
0xF878F8,
0xF85898,
0xF87858,
0xFCA044,
0xF8B800,
0xB8F818,
0x58D854,
0x58F898,
0x00E8D8,
0x787878,
0x000000,
0x000000,
0xFCFCFC,
0xA4E4FC,
0xB8B8F8,
0xD8B8F8,
0xF8B8F8,
0xF8A4C0,
0xF0D0B0,
0xFCE0A8,
0xF8D878,
0xD8F878,
0xB8F8B8,
0xB8F8D8,
0x00FCFC,
0xF8D8F8,
0x000000,
0x000000
};
uint8_t lastPixelValue;

static int TopBottomPercentage[12]={64,104,128,128,128,104,64,24,0,0,0,24};

uint8_t activeNTSCSignalLow;
uint8_t activeNTSCSignalHi;
uint8_t activeNTSCDisplay=1;
uint8_t activeNTSCSignalCol;

#define SIGNAL_OFFSET	(60)
#define SIGNAL_RANGE	(130)

uint16_t avgNTSC[3];
uint16_t sampleOld[3];

uint16_t waveTable[16]={0,0,1,1,2,2,3,3,2,2,1,1};

void GenerateNTSC(int colourClock)
{

#if 0
	int range = activeNTSCSignalHi-activeNTSCSignalLow;
	int inRangeR=((colourClock+0+5)%12)<6;
	int inRangeG=((colourClock+4+5)%12)<6;
	int inRangeB=((colourClock+8+5)%12)<6;
	int actualLevel=(range*TopBottomPercentage[(colourClock+activeNTSCSignalCol)%12])/128;
/*
	if (((regs2C02[1]&0x20 && inRangeR) ||
	    (regs2C02[1]&0x40 && inRangeG) ||
	    (regs2C02[1]&0x80 && inRangeB)) && activeNTSCDisplay)
	{
		actualLevel*=.546f;
		actualLevel-=SIGNAL_RANGE*.0902f;
	}
//	else
//		actualLevel+=15;
*/
	actualLevel+=activeNTSCSignalLow;
//	if (actualLevel<0)
//		printf("Level : %d\n",actualLevel);

//	printf("Level : %02X\n",actualLevel);
#else
	int actualLevel=activeNTSCSignalHi-activeNTSCSignalLow;

	actualLevel>>=waveTable[(colourClock+activeNTSCSignalCol)%12];
	
	actualLevel+=activeNTSCSignalLow;
#endif
	sampleOld[NTSCClock]=actualLevel;
#if ENABLE_LOGIC_ANALYSER
	RecordPin(4,actualLevel);
#endif
}

void writeNTSC()
{
	uint8_t actualLevel=(sampleOld[0]+sampleOld[1]+sampleOld[2])/3;
	if (!edlVideoGeneration)
		ntscDecodeAddSample(actualLevel);
//	printf("OLD : %d\n",actualLevel);
//	fwrite(&actualLevel,1,1,ntsc_file);
}


void SampleNTSC(uint8_t actualLevel)
{
/*
	int range = activeNTSCSignalHi-activeNTSCSignalLow;
	int inRangeR=((colourClock+0+5)%12)<6;
	int inRangeG=((colourClock+4+5)%12)<6;
	int inRangeB=((colourClock+8+5)%12)<6;
	int actualLevel=(range*TopBottomPercentage[(colourClock+activeNTSCSignalCol)%12])/128;

	if (((regs2C02[1]&0x20 && inRangeR) ||
	    (regs2C02[1]&0x40 && inRangeG) ||
	    (regs2C02[1]&0x80 && inRangeB)) && activeNTSCDisplay)
	{
		actualLevel*=.546f;
		actualLevel-=SIGNAL_RANGE*.0902f;
	}
//	else
//		actualLevel+=15;

	actualLevel+=activeNTSCSignalLow;
	if (actualLevel<0)
		printf("Level : %d\n",actualLevel);
*/
//	printf("Level : %02X\n",actualLevel);
	avgNTSC[NTSCClock]=actualLevel;
#if ENABLE_LOGIC_ANALYSER
	RecordPin(3,actualLevel);
#endif
	//printf("SAM : %d\n",actualLevel);
}

void AvgNTSC()
{
	uint8_t actualLevel=(avgNTSC[0]+avgNTSC[1]+avgNTSC[2])/3;
	if (edlVideoGeneration)
		ntscDecodeAddSample(actualLevel);
//	printf("AVG : %d\n",actualLevel);
//	fwrite(&actualLevel,1,1,ntsc_file);
}

void PPU_SetVideo(uint8_t x,uint8_t y,uint8_t col)
{
	uint32_t* outputTexture = (uint32_t*)videoMemory[MAIN_WINDOW];

	outputTexture[y*WIDTH+x]=nesColours[col];
	lastPixelValue=col;
}

void Tick2C02()
{
	static int triCnt=0;
	uint16_t curLine=0;
	uint16_t curClock=0;

	curLine=PPU_vClock&0x1FF;
	curClock=PPU_hClock&0x1FF;

	/// NTSC
	{
		if (/*curLine>17 && */curLine>=8 && curLine<11)
		{
			activeNTSCSignalLow=4;
			activeNTSCSignalHi=4;
		}
		else
		// For Every Line (for Now)
		if (curClock<256)			// active
		{
/*Synch	 	0.781	 0.000	 -0.359
Colorburst L	 1.000	 0.218	 -0.208
Colorburst H	 1.712	 0.931	 0.286
Color 0D	 1.131	 0.350	 -0.117
Color 1D (black) 1.300	 0.518	 0.000
Color 2D	 1.743	 0.962	 0.308
Color 3D	 2.331	 1.550	 0.715
Color 00	 1.875	 1.090	 0.397
Color 10	 2.287	 1.500	 0.681
Color 20	 2.743	 1.960	 1.000
Color 30	 2.743	 1.960	 1.000
*/

			static uint8_t levelsLow[4]={SIGNAL_OFFSET+(-.117f*SIGNAL_RANGE),SIGNAL_OFFSET+(.0f*SIGNAL_RANGE),SIGNAL_OFFSET+(.308f*SIGNAL_RANGE),SIGNAL_OFFSET+(.815f*SIGNAL_RANGE)};
			static uint8_t levelsHigh[4]={SIGNAL_OFFSET+(.397f*SIGNAL_RANGE),SIGNAL_OFFSET+(.681f*SIGNAL_RANGE),SIGNAL_OFFSET+(1.0f*SIGNAL_RANGE),SIGNAL_OFFSET+(1.0f*SIGNAL_RANGE)};


			lastPixelValue=0x16;

			uint8_t level=(lastPixelValue&0x30)>>4;
			uint8_t colour=lastPixelValue&0x0F;

			if (colour>13)
				level=1;

			uint8_t levelLow=levelsLow[level]/*(level<<0)*/;
			uint8_t levelHigh=levelsHigh[level]/*(level<<6)*/;

//			uint8_t levelLow=SIGNAL_OFFSET+((level+1)<<3);
//			uint8_t levelHigh=SIGNAL_OFFSET+((level+1)<<6);

			if (colour==0)
			{
				levelLow=levelHigh;
			}
			if (colour>12)
			{
				levelHigh=levelLow;
			}
			activeNTSCSignalLow=levelLow;
			activeNTSCSignalHi=levelHigh;
			activeNTSCSignalCol=colour+5;
			activeNTSCDisplay=1;
		}
		else
		if (curClock<256+11)
		{
			lastPixelValue=0x0F;
			activeNTSCSignalLow=70;
			activeNTSCSignalHi=70;
			activeNTSCDisplay=0;
			// background colour
		}
		else
		if (curClock<256+11+9)
		{
			activeNTSCSignalLow=60;
			activeNTSCSignalHi=60;
			// black
		}
		else
		if (curClock<256+11+9+25)
		{
			activeNTSCSignalLow=4;
			activeNTSCSignalHi=4;
			// sync
		}
		else
		if (curClock<256+11+9+25+4)
		{
			activeNTSCSignalLow=60;
			activeNTSCSignalHi=60;
			// black
		}
		else
		if (curClock<256+11+9+25+4+15)
		{
			activeNTSCSignalLow=60+(-.208f*SIGNAL_RANGE);
			activeNTSCSignalHi=60+(.286f*SIGNAL_RANGE);
			activeNTSCSignalCol=8;
			// colour burst
		}
		else
		if (curClock<256+11+9+25+4+15+5)
		{
			activeNTSCSignalLow=60;
			activeNTSCSignalHi=60;
			// black
		}
		else
		if (curClock<256+11+9+25+4+15+5+1)
		{
			activeNTSCSignalLow=60;
			activeNTSCSignalHi=60;
			// pulse???
		}
		else
		if (curClock<256+11+9+25+4+15+5+1+15)
		{
			activeNTSCSignalLow=70;
			activeNTSCSignalHi=70;
			// background colour
		}
		else
			printf("hiccup\n");
	}
}

//////////////////////// NOISES //////////////////////////

#define NUMBUFFERS            (2)				/* living dangerously*/

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

#define BUFFER_LEN		(44100/60)

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

int16_t currentDAC[4] = {0,0,0,0};

void _AudioAddData(int channel,int16_t dacValue)
{
	currentDAC[channel]=dacValue;
}

uint32_t tickCnt=0;
uint32_t tickRate=((341*262*2*4)/(44100/60));

/* audio ticked at same clock as everything else... so need a step down */
void UpdateAudio()
{
	tickCnt+=1;
	
	if (tickCnt>=tickRate*60)
	{
		tickCnt-=tickRate;

		if (amountAdded!=BUFFER_LEN)
		{
			int32_t res=0;

			res+=currentDAC[0];
			res+=currentDAC[1];
			res+=currentDAC[2];
			res+=currentDAC[3];

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

unsigned char * LoadFirstFileInZip(const char* path,unsigned int *length);

unsigned char *qLoad(const char *romName,uint32_t *length)
{
	FILE *inRom;
	unsigned char *romData=NULL;
	*length=0;

	romData = LoadFirstFileInZip(romName,length);
	if (romData!=0)
		return romData;

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

void LoadCart(const char* fileName)
{
	uint32_t length;
	uint8_t* ptr;
	uint8_t prgSize;
	uint8_t chrSize;
	uint8_t flags6;
	uint8_t flags7;
	uint8_t prgRamSize;
	uint8_t flags9;
	uint8_t flags10;
	uint8_t mapper;

	ptr=qLoad(fileName,&length);
	if (ptr)
	{
		if (ptr[0]!=0x4E ||		// Not an iNes Header
		    ptr[1]!=0x45 ||
		    ptr[2]!=0x53 ||
		    ptr[3]!=0x1A)
		{
			printf("Not in iNES format\n");
			exit(-1);
		}

		prgSize=ptr[4];
		chrSize=ptr[5];
		flags6=ptr[6];
		flags7=ptr[7];
		prgRamSize=ptr[8];
		flags9=ptr[9];
		flags10=ptr[10];

		mapper=(flags7&0xF0)|((flags6&0xF0)>>4);

		printf("prgSize : %08X\n",prgSize*16384);
		printf("chrSize : %08X\n",chrSize*8192);
		printf("flags6 : %02X\n",flags6);
		printf("flags7 : %02X\n",flags7);
		printf("prgRamSize : %02X\n",prgRamSize);
		printf("flags9 : %02X\n",flags9);
		printf("flags10 : %02X\n",flags10);
		printf("mapper : %02X\n",mapper);
		

		if (mapper>4)
		{
			printf("Only supports mapper 0-4!\n");
			exit(-1);
		}
		usingMMC1=mapper==1;
		usingUxROM=mapper==2;
		usingCNROM=mapper==3;
		usingMMC3=mapper==4;
		if ((flags6&0x0C)!=0)
		{
			printf("No Train/4Scr Support\n");
			exit(-1);
		}
		if (flags6&0x01)
		{
			hMirror=0;
			vMirror=1;
		}
		else
		{
			hMirror=1;
			vMirror=0;
		}

		prgRomSize=prgSize*16384;
		if (chrSize==0)
		{
			chrRomSize=8192;
			chrRom=chrRam;
		}
		else
		{
			chrRomSize=chrSize*8192;
			chrRom=&ptr[16+prgRomSize];
		}
		prgRom=&ptr[16];
		bnk0prg=prgRom;
		bnk1prg=prgRom+8192;
		bnk2prg=&ptr[16+((prgSize-1)*16384)];
		bnk3prg=bnk2prg+8192;

		bnk00chr=chrRom;
		bnk01chr=chrRom+0x400;
		bnk02chr=chrRom+0x800;
		bnk03chr=chrRom+0xC00;
		bnk10chr=chrRom+0x1000;
		bnk11chr=chrRom+0x1400;
		bnk12chr=chrRom+0x1800;
		bnk13chr=chrRom+0x1C00;
	}
}
