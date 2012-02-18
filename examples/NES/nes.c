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

#define ENABLE_TV		0
#define ENABLE_LOGIC_ANALYSER	0
#define ENABLE_DEBUGGER		1
#define ENABLE_LOG_TRACE	0

#include "jake\ntscDecode.h"

int g_instructionStep=0;
int stopTheClock=1;

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

extern uint8_t PPU_colourCounter;

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
uint8_t hvMirror;

int usingMMC1=0;
int usingMMC3=0;
int usingUxROM=0;
int usingCNROM=0;
int usingColorDreams=0;
int usingAOROM=0;

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

int InitialiseMemory()
{
	return 0;
}

uint8_t internalRam[0x800];
uint8_t ppuRam[0x800];

void APUSetByte(uint16_t addr,uint8_t byte);
uint8_t APUGetByte(uint16_t addr);

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
	if (hvMirror)
		chrAddr+=0x400;
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
	if (hvMirror)
		chrAddr+=0x400;
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

uint8_t mapperIRQ=0;

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
		if (addr>=0x4000 && addr<=0x4017)
		{
			return APUGetByte(addr);
		}
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

void WriteMMC1_SwitchChr0()
{
	if (MMC1_Control&0x10)
	{
		bnk00chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1F)];
		bnk01chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1F)+0x400];
		bnk02chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1F)+0x800];
		bnk03chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1F)+0xC00];
	}
	else
	{
		bnk00chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1E)];
		bnk01chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1E)+0x400];
		bnk02chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1E)+0x800];
		bnk03chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1E)+0xC00];
		bnk10chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1E)+0x1000];
		bnk11chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1E)+0x1400];
		bnk12chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1E)+0x1800];
		bnk13chr=&chrRom[0x1000*(MMC1_ChrBank0&0x1E)+0x1C00];
	}
}

void WriteMMC1_SwitchChr1()
{
	if (MMC1_Control&0x10)
	{
		bnk10chr=&chrRom[0x1000*(MMC1_ChrBank1&0x1F)];
		bnk11chr=&chrRom[0x1000*(MMC1_ChrBank1&0x1F)+0x400];
		bnk12chr=&chrRom[0x1000*(MMC1_ChrBank1&0x1F)+0x800];
		bnk13chr=&chrRom[0x1000*(MMC1_ChrBank1&0x1F)+0xC00];
	}
}

void WriteMMC1_SwitchPrg()
{
	switch (MMC1_Control&0x0C)
	{
		case 0:
		case 0x4:
			bnk0prg=&prgRom[MMC1_PrgBank*16384];
			bnk1prg=&prgRom[MMC1_PrgBank*16384 + 8192];
			bnk2prg=&prgRom[MMC1_PrgBank*16384 + 16384];
			bnk3prg=&prgRom[MMC1_PrgBank*16384 + 16384 + 8192];
			break;
		case 0x8:
			bnk0prg=&prgRom[0];
			bnk1prg=bnk0prg+8192;
			bnk2prg=&prgRom[MMC1_PrgBank*16384];
			bnk3prg=&prgRom[MMC1_PrgBank*16384+8192];
			break;
		case 0xC:
			bnk0prg=&prgRom[MMC1_PrgBank*16384];
			bnk1prg=&prgRom[MMC1_PrgBank*16384+8192];
			bnk2prg=&prgRom[prgRomSize-16384];
			bnk3prg=bnk2prg+8192;
			break;
	}

}

void WriteMMC1_Control(uint8_t byte)	//NB need to set bank configs up here as well!!!
{
	byte&=0x1F;
	if (MMC1_Control!=byte)
		printf("MMC1 control new value %02X\n",byte);
	MMC1_Control=byte;
	switch (MMC1_Control&3)
	{
		case 0:
			hvMirror=0;
			hMirror=0;
			vMirror=0;
			break;
		case 1:
			hvMirror=1;
			hMirror=0;
			vMirror=0;
			break;
		case 2:
			hvMirror=0;
			hMirror=0;
			vMirror=1;
			break;
		case 3:
			hvMirror=0;
			hMirror=1;
			vMirror=0;
			break;
	}

	WriteMMC1_SwitchChr0();
	WriteMMC1_SwitchChr1();
	WriteMMC1_SwitchPrg();
}

void WriteMMC1_ChrBank0(uint8_t byte)
{
	byte&=0x1F;

	if (MMC1_ChrBank0!=byte)
		printf("MMC1 char0 bank %02X\n",byte);
	MMC1_ChrBank0=byte;
	MMC1_ChrBank0&=(chrRomSize*2)-1;
	WriteMMC1_SwitchChr0();
}

void WriteMMC1_ChrBank1(uint8_t byte)
{
	byte&=0x1F;
	if (MMC1_ChrBank1!=byte)
		printf("MMC1 char1 bank %02X  :  Mask : %08X\n",byte,(chrRomSize*2)-1);
	MMC1_ChrBank1=byte;
	MMC1_ChrBank1&=(chrRomSize*2)-1;
	WriteMMC1_SwitchChr1();
}
void WriteMMC1_PrgBank(uint8_t byte)
{
	//DBBBB  D-PRGRam Disable  - BBBB is bank
//	if (MMC1_PrgBank!=byte)
//		printf("MMC1 prg bank %02X : %08X\n",byte,(prgRomSize/16384)-1);
	MMC1_PrgBank=byte&0xF;
	MMC1_PrgBank&=(prgRomSize/16384)-1;
	WriteMMC1_SwitchPrg();
}

void WriteToUxROM(uint16_t addr,uint8_t byte)
{
	byte&=0x7;
	byte&=(prgRomSize/16384)-1;
	bnk0prg=&prgRom[byte*16384];
	bnk1prg=&prgRom[byte*16384+8192];
}

void WriteToCNROM(uint16_t addr,uint8_t byte)
{
	byte&=0x3;
	byte&=chrRomSize-1;
	bnk00chr=&chrRom[byte*0x2000];
	bnk01chr=&chrRom[byte*0x2000+0x400];
	bnk02chr=&chrRom[byte*0x2000+0x800];
	bnk03chr=&chrRom[byte*0x2000+0xC00];
	bnk10chr=&chrRom[byte*0x2000+0x1000];
	bnk11chr=&chrRom[byte*0x2000+0x1400];
	bnk12chr=&chrRom[byte*0x2000+0x1800];
	bnk13chr=&chrRom[byte*0x2000+0x1C00];
}

void WriteToColorDreams(uint16_t addr,uint8_t byte)
{
	//CCCCLLPP	- C chrrom bank | P prgrom bank
	uint8_t chr=(byte&0xF0)>>4;
	uint8_t prg=(byte&0x03);
	chr&=chrRomSize-1;
	prg&=((prgRomSize/16384)*2)-1;

	bnk0prg=&prgRom[prg*32768];
	bnk1prg=&prgRom[prg*32768+8192];
	bnk2prg=&prgRom[prg*32768+16384];
	bnk3prg=&prgRom[prg*32768+16384+8192];

	bnk00chr=&chrRom[chr*0x2000];
	bnk01chr=&chrRom[chr*0x2000+0x400];
	bnk02chr=&chrRom[chr*0x2000+0x800];
	bnk03chr=&chrRom[chr*0x2000+0xC00];
	bnk10chr=&chrRom[chr*0x2000+0x1000];
	bnk11chr=&chrRom[chr*0x2000+0x1400];
	bnk12chr=&chrRom[chr*0x2000+0x1800];
	bnk13chr=&chrRom[chr*0x2000+0x1C00];
}

void WriteToAOROM(uint16_t addr,uint8_t byte)
{
	//xxxSPPPP	- S mirror control | P prgrom bank
	uint8_t prg=(byte&0x0F);
	prg&=(prgRomSize/32768)-1;

	bnk0prg=&prgRom[prg*32768];
	bnk1prg=&prgRom[prg*32768+8192];
	bnk2prg=&prgRom[prg*32768+16384];
	bnk3prg=&prgRom[prg*32768+16384+8192];

	if (byte&0x10)
	{
		hvMirror=1;
		hMirror=0;
		vMirror=0;
	}
	else
	{
		hvMirror=0;
		hMirror=0;
		vMirror=0;
	}
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
uint8_t MMC3_IRQLatch=0;
uint8_t MMC3_IRQCount=0;
uint8_t MMC3_IRQDisable=1;
uint8_t MMC3_IRQPending=0;

uint8_t MMC3_TickIRQ()
{
	if (MMC3_IRQDisable)
		return 1;

	static lastClock=59;
	if (PPU_vClock!=lastClock)
	{
		lastClock=PPU_vClock;
		if (MMC3_IRQCount==0)
		{
			MMC3_IRQPending=1;
			MMC3_IRQCount=MMC3_IRQLatch;
		}
		MMC3_IRQCount--;
	}
	return MMC3_IRQPending;
}

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
		bnk10chr=&chrRom[(MMC3_BankData[0]&0xFE)*1024];
		bnk11chr=&chrRom[(MMC3_BankData[0]&0xFE)*1024+0x400];
		bnk12chr=&chrRom[(MMC3_BankData[1]&0xFE)*1024];
		bnk13chr=&chrRom[(MMC3_BankData[1]&0xFE)*1024+0x400];
	}
	else
	{
		// R0|R0+|R1|R1+|R2|R3|R4|R5
		bnk00chr=&chrRom[(MMC3_BankData[0]&0xFE)*1024];
		bnk01chr=&chrRom[(MMC3_BankData[0]&0xFE)*1024+0x400];
		bnk02chr=&chrRom[(MMC3_BankData[1]&0xFE)*1024];
		bnk03chr=&chrRom[(MMC3_BankData[1]&0xFE)*1024+0x400];
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
	WriteMMC3_SwitchChr();
}

void WriteMMC3_BankData(uint8_t byte)
{
	if ((MMC3_BankSel&7)<6)
	{
		MMC3_BankData[MMC3_BankSel&7]=byte&((chrRomSize*8)-1);
		WriteMMC3_SwitchChr();
	}
	else
	{
		MMC3_BankData[MMC3_BankSel&7]=byte&(((prgRomSize/16384)*2)-1);
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
//	printf("MMC3 Write : %04X,%02X\n",addr,byte);
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
		case 0xC000:
			MMC3_IRQLatch=byte;
			break;
		case 0xC001:
			MMC3_IRQCount=0;
			break;
		case 0xE000:
			MMC3_IRQDisable=1;
			MMC3_IRQPending=0;
			break;
		case 0xE001:
			MMC3_IRQDisable=0;
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
		if (addr>=0x4000 && addr<=0x4017)
		{
			APUSetByte(addr,byte);
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
	if (usingColorDreams)
	{
		WriteToColorDreams(addr,byte);
	}
	if (usingAOROM)
	{
		WriteToAOROM(addr,byte);
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

uint32_t MAIN_missing(uint32_t opcode)
{
	printf("MAIN:Executing Missing Instruction!! : %04X:%02X\n",MAIN_PC-1,opcode);
	stopTheClock=1;
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
	int first=1;
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
				if (first)
				{
					first=0;
					sprintf(sprintBuffer,"$%02X",GetByte(address+offset));
				}
				else
				{
					sprintf(sprintBuffer,"%02X",GetByte(address+offset));
				}
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

uint8_t APU_Interrupt=1;			// This is the line to the cpu
uint8_t APU_InterruptPending=0;			// This is the frame interrupt flag

void ClockCPU(int cpuClock)
{
	uint16_t addr; 
	{
		MAIN_PinSetCLK(cpuClock&1);

		uint8_t m2=MAIN_PinGetM2()&1;

#if ENABLE_LOGIC_ANALYSER
//		RecordPin(0,cpuClock&1);
		RecordPin(1,MAIN_PinGetM2()&1);
#endif
		addr = MAIN_PinGetA();

#if ENABLE_DEBUGGER
		static int lastDebugSync=0;
		if ((lastDebugSync==0) && MAIN_DEBUG_SYNC)
		{
			lastPC=addr;
			if (g_instructionStep)
				g_instructionStep=0;
#if ENABLE_LOG_TRACE
			int numBytes=0;
			const char* retVal = decodeDisasm(MAIN_DIS_,lastPC,&numBytes,255);
			int opcodeLength=numBytes+1;
			int a;

			printf("%04X  ",lastPC);

			for (a=0;a<opcodeLength;a++)
			{
				printf("%02X ",GetByte(lastPC+a));
			}
			for (a=opcodeLength;a<3;a++)
			{
				printf("   ");
			}
			printf(" ");
			printf("%s",retVal);
			for (a=strlen(retVal);a<40-7;a++)
			{
				printf(" ");
			}
			printf("A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%3d SL:%3d\n",MAIN_A,MAIN_X,MAIN_Y,MAIN_P,MAIN_SP&0xFF,PPU_hClock,PPU_vClock);
#endif
			if (isBreakpoint(0,lastPC))
			{
				stopTheClock=1;
			}
		}
		lastDebugSync=MAIN_DEBUG_SYNC;
#endif
		// Phase is 1 (LS139 decode gives us DBE low when Phase 1, Address=001x xxxx xxxx xxxx
		uint8_t dbe_signal=(m2) && ((addr&0x8000)==0) && ((addr&0x4000)==0) && ((addr&0x2000)==0x2000)?0:1;
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
			if (lastM2!=m2)
			{
				lastM2=m2;
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

	}

}

void ClockPPU(int ppuClock)
{
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

#if ENABLE_LOGIC_ANALYSER
		RecordPin(0,(PPU_PinGetPA()&0x10)>>4);
#endif
	}

}

uint32_t APU_Divider=0;
uint32_t APU_CPUClockRate=0;

uint8_t APU_FrameControl=0;
uint8_t APU_FrameSequence=0;

uint8_t APU_Status=0;

uint8_t APU_Begin;
uint8_t APU_NextFrameControl;

uint8_t APU_oddCycle=1;

uint8_t APU_Timer[4]={0,0,0,0};
uint16_t APU_WaveTimer[4]={0,0,0,0};
uint16_t APU_WaveTimerActual[4]={0,0,0,0};
uint8_t APU_Counter[4]={0,0,0,0};

uint8_t APU_LinearCounter=0;
uint8_t APU_LinearHalt=0;
uint8_t APU_TriSequencer=0;

uint8_t APU_NoiseModePeriod=0;

uint8_t APU_Sequence0[6]={0x80,0xC0,0x80,0x20,0xF0,0x21};		//0x80 - clock envelopes : 0x40 - clock length counters and sweeps : 0x20 - set interrupt flag : 0x10 - generate irq : 1 - reset divider
uint8_t APU_Sequence1[6]={0x80,0xC0,0x80,0x00,0xC0,0x01};

uint32_t APU_SequenceClock0[6]={7458,7458*2-1,7458*3,29830,29831,29832};
uint32_t APU_SequenceClock1[6]={7458,7458*2-1,7458*3,7458*4,7458*5-7,7458*5-6};

void APU_UpdateFrameSequence(uint8_t flag)
{
	if (flag&0x80)
	{
		//clock envelopes + linear counter
		if (APU_LinearHalt)
		{
			APU_LinearCounter=APU_Timer[2]&0x7F;
		}
		else
		{
			if (APU_LinearCounter!=0)
				APU_LinearCounter--;
		}
		if ((APU_Timer[2]&0x80)==0)
			APU_LinearHalt=0;
	}
	if (flag&0x40)
	{
		//clock lengths + sweeps
		if ((APU_Timer[0]&0x20)==0 && APU_Counter[0]!=0)
		{
			APU_Counter[0]--;
		}
		if ((APU_Timer[1]&0x20)==0 && APU_Counter[1]!=0)
		{
			APU_Counter[1]--;
		}
		if ((APU_Timer[2]&0x80)==0 && APU_Counter[2]!=0)
		{
			APU_Counter[2]--;
		}
		if ((APU_Timer[3]&0x20)==0 && APU_Counter[3]!=0)
		{
			APU_Counter[3]--;
		}
	}
	if (flag&0x20)
	{
		// set i flag
		if ((APU_FrameControl&0x40)==0)
		{
			APU_InterruptPending=1;
		}
	}
	if (flag&0x10)
	{
		if (APU_InterruptPending)		// This probably should be a marker - otherwise we might miss later interrupts...
		{
			APU_Interrupt=0;
		}
	}

	if (flag&0x01)
	{
		APU_Divider=2;
		APU_FrameSequence=0xFF;		// will be ticked on exit
	}
}

void APU_WriteFrameControl(uint8_t byte)
{
	APU_FrameControl=byte;
	APU_Divider=0;
	APU_FrameSequence=0;
	if (APU_FrameControl&0x80)
	{
		APU_UpdateFrameSequence(0xC0);
	}
	if (APU_FrameControl&0x40)
	{
		APU_InterruptPending=0;
		APU_Interrupt=1;
	}
}


uint8_t APUGetByte(uint16_t addr)
{
	switch (addr)
	{
		case 0x4000:
		case 0x4001:
		case 0x4002:
		case 0x4003:
		case 0x4004:
		case 0x4005:
		case 0x4006:
		case 0x4007:
		case 0x4008:
		case 0x4009:
		case 0x400A:
		case 0x400B:
		case 0x400C:
		case 0x400D:
		case 0x400E:
		case 0x400F:
		case 0x4010:
		case 0x4011:
		case 0x4012:
		case 0x4013:
		case 0x4014:
			break;
		case 0x4015:
			{
				uint8_t ret=0;
				
				if (APU_Counter[0]!=0)
					ret|=1;
				if (APU_Counter[1]!=0)
					ret|=2;
				if (APU_Counter[2]!=0)
					ret|=4;
				if (APU_Counter[3]!=0)
					ret|=8;
				if ((APU_FrameControl&0x40)==0)
				{
					if (APU_InterruptPending)
					{
						ret|=0x40;
					}
				}
				APU_InterruptPending=0;
				APU_Interrupt=1;

				return ret;
			}
			break;
		case 0x4016:
		{
			uint8_t tmp=controllerData&0x80;
			controllerData<<=1;
			tmp>>=7;
			return tmp;
		}
		case 0x4017:
			break;
	}

	return 0;
}


/*    bits  bit 3
    7-4   0   1
        -------
    0   $0A $FE
    1   $14 $02
    2   $28 $04
    3   $50 $06
    4   $A0 $08
    5   $3C $0A
    6   $0E $0C
    7   $1A $0E
    8   $0C $10
    9   $18 $12
    A   $30 $14
    B   $60 $16
    C   $C0 $18
    D   $48 $1A
    E   $10 $1C
    F   $20 $1E*/


uint8_t counterReset0[16]={0x0A,0x14,0x28,0x50,0xA0,0x3C,0x0E,0x1A,0x0C,0x18,0x30,0x60,0xC0,0x48,0x10,0x20};
uint8_t counterReset1[16]={0xFE,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E};

void APU_WriteLength(uint8_t enMask,uint8_t chn,uint8_t byte)	// need to do multi channel!
{
	uint8_t table=(byte&0x08)>>3;
	uint8_t index=(byte&0xF0)>>4;

	if (APU_Status&enMask)
	{
		if (table)
		{
			APU_Counter[chn]=counterReset1[index];
		}
		else
		{
			APU_Counter[chn]=counterReset0[index];
		}
	}		

}

void APUSetByte(uint16_t addr,uint8_t byte)
{
	switch (addr)
	{
		case 0x4000:
			// Pulse1Timer	ttedvvvv		t - duty cycle type    e - length counter disable / envelope loop enable    d - envelope decay disable    v - volume/decay rate
			// 		00010001
			APU_Timer[0]=byte;
			break;
		case 0x4001:
			break;
		case 0x4002:
			APU_WaveTimer[0]&=0x700;
			APU_WaveTimer[0]|=byte;
			break;
		case 0x4003:
			// Pulse1Length	ccccclll
			// 		00011000
			APU_WaveTimer[0]&=0x0FF;
			APU_WaveTimer[0]|=(byte&7)<<8;
			APU_WriteLength(1,0,byte);
			break;
		case 0x4004:
			// Pulse2Timer	ttedvvvv		t - duty cycle type    e - length counter disable / envelope loop enable    d - envelope decay disable    v - volume/decay rate
			// 		00010001
			APU_Timer[1]=byte;
			break;
		case 0x4005:
			break;
		case 0x4006:
			APU_WaveTimer[1]&=0x700;
			APU_WaveTimer[1]|=byte;
			break;
		case 0x4007:
			// Pulse2Length	ccccclll
			// 		00011000
			APU_WaveTimer[1]&=0x0FF;
			APU_WaveTimer[1]|=(byte&7)<<8;
			APU_WriteLength(2,1,byte);
			break;
		case 0x4008:
			// TriangleTimer	dlllllll		d - length counter disable    l - linear load register
			APU_Timer[2]=byte;
			break;
		case 0x4009:
			break;
		case 0x400A:
			APU_WaveTimer[2]&=0x700;
			APU_WaveTimer[2]|=byte;
			break;
		case 0x400B:
			// TriangleLength	ccccclll
			APU_WaveTimer[2]&=0x0FF;
			APU_WaveTimer[2]|=(byte&7)<<8;
			APU_LinearHalt=1;
			APU_WriteLength(4,2,byte);
			break;
		case 0x400C:
			// NoiseTimer	ttedvvvv		t - duty cycle type    e - length counter disable / envelope loop enable    d - envelope decay disable    v - volume/decay rate
			// 		00010001
			APU_Timer[3]=byte;
			break;
		case 0x400D:
			break;
		case 0x400E:
			APU_NoiseModePeriod=byte;
			break;
		case 0x400F:
			// Pulse2Length	ccccclll
			// 		00011000
			APU_WriteLength(8,3,byte);
			break;
		case 0x4010:
			break;
		case 0x4011:
			break;
		case 0x4012:
			break;
		case 0x4013:
			break;
		case 0x4014:
			break;
		case 0x4015:
			APU_Status=byte;
			if ((byte&1)==0)
			{
				APU_Counter[0]=0;
			}
			if ((byte&2)==0)
			{
				APU_Counter[1]=0;
			}
			if ((byte&4)==0)
			{
				APU_Counter[2]=0;
			}
			if ((byte&8)==0)
			{
				APU_Counter[3]=0;
			}
			break;
		case 0x4016:
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
			break;
		case 0x4017:
		{
			APU_Begin=APU_oddCycle*1+1;
			APU_NextFrameControl=byte;
			break;
		}
	}
}

uint8_t APU_Duty0[16]={1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t APU_Duty1[16]={1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t APU_Duty2[16]={1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0};
uint8_t APU_Duty3[16]={1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0};

uint8_t DAC_LEVEL[4]={0,0,0,0};

uint8_t APU_DutyCycle[2]={0,0};

void GenerateSqr1()
{
	// TIMER
	if (APU_WaveTimerActual[0]==0)
	{
		APU_WaveTimerActual[0]=APU_WaveTimer[0];
		// SEQUENCER
		APU_DutyCycle[0]++;

		switch (APU_Timer[0]&0xC0)
		{
			case 0x00:
				DAC_LEVEL[0]=APU_Duty0[APU_DutyCycle[0]&0xF];
				break;
			case 0x40:
				DAC_LEVEL[0]=APU_Duty1[APU_DutyCycle[0]&0xF];
				break;
			case 0x80:
				DAC_LEVEL[0]=APU_Duty2[APU_DutyCycle[0]&0xF];
				break;
			case 0xC0:
				DAC_LEVEL[0]=APU_Duty3[APU_DutyCycle[0]&0xF];
				break;
		}
	}
	else
	{
		APU_WaveTimerActual[0]--;
	}

	// LENGTH
	if (APU_Counter[0]==0)
		DAC_LEVEL[0]=0;
}

void GenerateSqr2()
{
	// TIMER
	if (APU_WaveTimerActual[1]==0)
	{
		APU_WaveTimerActual[1]=APU_WaveTimer[1];
		// SEQUENCER
		APU_DutyCycle[1]++;

		switch (APU_Timer[1]&0xC0)
		{
			case 0x00:
				DAC_LEVEL[1]=APU_Duty0[APU_DutyCycle[1]&0xF];
				break;
			case 0x40:
				DAC_LEVEL[1]=APU_Duty1[APU_DutyCycle[1]&0xF];
				break;
			case 0x80:
				DAC_LEVEL[1]=APU_Duty2[APU_DutyCycle[1]&0xF];
				break;
			case 0xC0:
				DAC_LEVEL[1]=APU_Duty3[APU_DutyCycle[1]&0xF];
				break;
		}
	}
	else
	{
		APU_WaveTimerActual[1]--;
	}

	// LENGTH
	if (APU_Counter[1]==0)
		DAC_LEVEL[1]=0;
}

uint8_t triangleSequencer[32]={0x0F,0x0E,0x0D,0x0C,0x0B,0x0A,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F};

void GenerateTriangle()
{
	if (APU_WaveTimerActual[2]==0)
	{
		APU_WaveTimerActual[2]=APU_WaveTimer[2];

		if (APU_LinearCounter!=0 && APU_Counter[2]!=0)
		{
			APU_TriSequencer++;
			APU_TriSequencer&=31;
		}
	}
	else
	{
		APU_WaveTimerActual[2]--;
	}
		
	DAC_LEVEL[2]=triangleSequencer[APU_TriSequencer];
}

uint16_t noisePeriod[16]={0x004,0x008,0x010,0x020,0x040,0x060,0x080,0x0A0,0x0CA,0x0FE,0x17C,0x1FC,0x2FA,0x3F8,0x7F2,0xFE4};

uint16_t noiseShifter=1;

void GenerateNoise()
{
	// TIMER
	if (APU_WaveTimerActual[3]==0)
	{
		APU_WaveTimerActual[3]=noisePeriod[APU_NoiseModePeriod&0xF];

		uint16_t preShift;
		if (APU_NoiseModePeriod&0x80)
		{
			preShift=((noiseShifter&1)^((noiseShifter&0x40)>>5))<<13;
		}
		else
		{
			preShift=((noiseShifter&1)^((noiseShifter&2)>>1))<<13;
		}
		noiseShifter>>=1;
		noiseShifter|=preShift;

		DAC_LEVEL[3]=(~noiseShifter)&1;
	}
	else
	{
		APU_WaveTimerActual[3]--;
	}

	if (APU_Counter[3]==0)
		DAC_LEVEL[3]=0;
}

void ClockAPU(int apuClock)
{
	static int SLOWMO=0;

	// Temporary - needs moving into cpu core
	if (apuClock&1)
	{
		APU_CPUClockRate++;
		if (APU_CPUClockRate==6)
		{
			int a;
			APU_CPUClockRate=0;

			GenerateSqr1();
			GenerateSqr2();
			GenerateTriangle();
			GenerateNoise();
		}
	}

	if ((SLOWMO%24)==0)
	{
		APU_oddCycle^=1;

		if (APU_FrameControl&0x80)
		{
			if (APU_Divider==APU_SequenceClock1[APU_FrameSequence])
			{
				APU_UpdateFrameSequence(APU_Sequence1[APU_FrameSequence]);
				APU_FrameSequence++;
			}
		}
		else
		{
			if (APU_Divider==APU_SequenceClock0[APU_FrameSequence])
			{
				APU_UpdateFrameSequence(APU_Sequence0[APU_FrameSequence]);
				APU_FrameSequence++;
			}
		}

		if (APU_Begin)
		{
			APU_Begin--;
			if (APU_Begin==0)
			{
				APU_WriteFrameControl(APU_NextFrameControl);
			}
		}
		APU_Divider++;
	}
	SLOWMO++;

	_AudioAddData(0,DAC_LEVEL[0]?(APU_Timer[0]&0x0F)<<10:0);
	_AudioAddData(1,DAC_LEVEL[1]?(APU_Timer[1]&0x0F)<<10:0);
	_AudioAddData(2,DAC_LEVEL[2]<<10);
	_AudioAddData(3,DAC_LEVEL[3]?(APU_Timer[3]&0x0F)<<10:0);
}

void TickChips(int MasterClock)
{
	ClockCPU(MasterClock);
	ClockAPU(MasterClock);
	ClockPPU(MasterClock);
	MAIN_PinSet_IRQ((MMC3_TickIRQ()&APU_Interrupt)&1);
	MAIN_PinSet_NMI(PPU_PinGet_INT());

#if ENABLE_TV
	
	{
		SampleNTSC(PPU_PinGetVIDEO());
		if (NTSCClock==2)
		{
			AvgNTSC();
		}

		NTSCClock++;
		if (NTSCClock==3)
			NTSCClock=0;
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
	MAIN_PinSet_RST(0);
	MAIN_PinSet_RST(1);
//	MAIN_PC=GetByte(0xFFFC);
//	MAIN_PC|=GetByte(0xFFFD)<<8;

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


			TickChips(MasterClock);
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
				g_instructionStep=1;
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

uint16_t avgNTSC[3];

uint8_t lastEDLLevelAvg=0;

void SampleNTSC(uint8_t actualLevel)
{
	avgNTSC[NTSCClock]=actualLevel;
#if ENABLE_LOGIC_ANALYSER
	RecordPin(3,lastEDLLevelAvg);
#endif
}

void AvgNTSC()
{
	lastEDLLevelAvg=(avgNTSC[0]+avgNTSC[1]+avgNTSC[2])/3;
	ntscDecodeAddSample(lastEDLLevelAvg);
}

void PPU_SetVideo(uint8_t x,uint8_t y,uint8_t col)
{
	uint32_t* outputTexture = (uint32_t*)videoMemory[MAIN_WINDOW];

	outputTexture[y*WIDTH+x]=nesColours[col];
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

FILE* poop=NULL;

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

			if (!poop)
			{
				poop=fopen("out.raw","wb");
			}

			fwrite(&currentDAC[0],2,1,poop);
			fwrite(&currentDAC[1],2,1,poop);
			fwrite(&currentDAC[2],2,1,poop);
			fwrite(&currentDAC[3],2,1,poop);

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
		

		if (mapper>4 && mapper!=7 && mapper!=11)
		{
			printf("Only supports mapper 0-4,7,11!\n");
			exit(-1);
		}
		usingMMC1=mapper==1;
		usingUxROM=mapper==2;
		usingCNROM=mapper==3;
		usingMMC3=mapper==4;
		usingAOROM=mapper==7;
		usingColorDreams=mapper==11;
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
			chrRomSize=1;
			chrRom=chrRam;
		}
		else
		{
			chrRomSize=chrSize;
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
