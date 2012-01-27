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

#define USE_EDL_PPU	1

#include "jake\ntscDecode.h"


void AudioKill();
void AudioInitialise();
void UpdateAudio();
void _AudioAddData(int channel,int16_t dacValue);

extern uint8_t PPU_Control;
extern uint8_t PPU_Mask;
extern uint8_t PPU_Status;

void PPU_PinSetPIN_RW(uint8_t);
void PPU_PinSetPIN__DBE(uint8_t);
void PPU_PinSetPIN_RS(uint8_t);
void PPU_PinSetPIN_D(uint8_t);
void PPU_PinSetPIN_CLK(uint8_t);
uint8_t PPU_PinGetPIN_D();
uint8_t PPU_PinGetPIN_PA();
uint8_t PPU_PinGetPIN_AD();
uint8_t PPU_PinGetPIN_ALE();
uint8_t PPU_PinGetPIN__RE();
uint8_t PPU_PinGetPIN__WE();

uint16_t MAIN_PinGetPIN_AB();
uint8_t MAIN_PinGetPIN_DB();
void MAIN_PinSetPIN_DB(uint8_t);
void MAIN_PinSetPIN_O0(uint8_t);
void MAIN_PinSetPIN_SO(uint8_t);
uint8_t MAIN_PinGetPIN_RW();
void MAIN_PinSetPIN__IRQ(uint8_t);
void MAIN_PinSetPIN__RES(uint8_t);

uint8_t *prgRom;
uint32_t prgRomSize;
uint8_t *chrRom;
uint32_t chrRomSize;

uint8_t chrRam[8192];

uint8_t hMirror;
uint8_t vMirror;

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
uint8_t sprRam[0x100];
uint8_t regs2C02[8]={0,0,0,0,0,0,0,0};
//uint16_t ppuAddr;

uint16_t PPU_FV;		// 3 bits
uint16_t PPU_FVc;
uint16_t PPU_V;			// 1 bit
uint16_t PPU_Vc;
uint16_t PPU_H;			// 1 bit
uint16_t PPU_Hc;
uint16_t PPU_VT;		// 5 bits
uint16_t PPU_VTc;
uint16_t PPU_HT;		// 5 bits
uint16_t PPU_HTc;
uint16_t PPU_FH;		// 3 bits
uint16_t PPU_FHl;		// 3 bits		// latched cant change during scanline render
uint16_t PPU_S;			// 1 bits

uint8_t palIndex[0x20];

uint8_t PPUGetByte(uint16_t addr)
{
//	printf("FETCH FROM : %04X\n",addr);
	addr&=0x3FFF;
	if (addr<0x2000)
	{
		return chrRom[addr&0x1FFF];		// WILL NEED FIXING
	}
	if (addr<0x3F00)
	{
		uint16_t chrAddr=(addr-0x2000)&0x3FF;
		if (hMirror)
			chrAddr|=(addr&0x0800)>>1;
		if (vMirror)
			chrAddr|=(addr&0x0400);
		return ppuRam[chrAddr];		// WILL NEED FIXING
	}
	return palIndex[addr&0x1F];/// PALLETTE
}
void PPUSetByte(uint16_t addr,uint8_t byte)
{
	//printf("STORE TO : %04X (%02X)\n",addr,byte);
	addr&=0x3FFF;
	if (addr<0x2000)
	{
		if (chrRom==chrRam)
			chrRom[addr]=byte;
		return;		/// assuming fixed tile ram
	}
	if (addr<0x3F00)
	{
		uint16_t chrAddr=(addr-0x2000)&0x3FF;
		if (hMirror)
			chrAddr|=(addr&0x0800)>>1;
		if (vMirror)
			chrAddr|=(addr&0x0400);
		ppuRam[chrAddr]=byte;
		return;
	}
	switch (addr&0x1F)
	{
		case 0:
		case 0x10:
			palIndex[0x00]=byte&0x3F;
			return;
		default:
			palIndex[addr&0x1F]=byte&0x3F;
			return;
	}
}

uint8_t IORead(uint8_t addr)
{
	uint8_t value=regs2C02[addr];
	uint8_t ppuCmp=PPU_PinGetPIN_D();
	if (ppuCmp!=value)
	{
//		printf("Mismatch : %02X : %02X!=%02X\n",addr,value,ppuCmp);
	}
	switch (addr)
	{
		case 0:
		printf("IO Read : %d\n",addr&0x7);
			break;
		case 1:
		printf("IO Read : %d\n",addr&0x7);
			break;
		case 2:
			regs2C02[addr]&=0x7F;
			regs2C02[6]=0;	// reset address latch
			regs2C02[5]=0;	// reset address latch
			break;
		case 3:
		printf("IO Read : %d\n",addr&0x7);
			break;
		case 4:
			return sprRam[regs2C02[3]];
		case 5:
		printf("IO Read : %d\n",addr&0x7);
			break;
		case 6:
		printf("IO Read : %d\n",addr&0x7);
			break;
		case 7:
			{
			uint16_t ppuAddr=(PPU_FVc<<12)|(PPU_Vc<<11)|(PPU_Hc<<10)|(PPU_VTc<<5)|PPU_HTc;
			regs2C02[addr]=PPUGetByte(ppuAddr);
			if ((ppuAddr&0x3FFF)>0x3eFF)
			{
				value=regs2C02[addr];
			}
			if (regs2C02[0]&0x04)
			{
				ppuAddr+=32;
			}
			else
			{
				ppuAddr+=1;
			}
			PPU_FVc=(ppuAddr&0x7000)>>12;
			PPU_Vc=(ppuAddr&0x0800)>>11;
			PPU_Hc=(ppuAddr&0x0400)>>10;
			PPU_VTc=(ppuAddr&0x03E0)>>5;
			PPU_HTc=(ppuAddr&0x001F);
/*			if (regs2C02[0]&0x04)
				ppuAddr+=32;
			else
				ppuAddr++;*/
			}
			break;
	}
	return value;
}

void IOWrite(uint8_t addr,uint8_t byte)
{
	switch (addr)
	{
		case 0:
			PPU_V=(byte&0x02)>>1;
			PPU_H=(byte&0x01);
			PPU_S=(byte&0x10)>>4;
		case 1:
//			if (addr==1)
//				printf("001-Byte : %02X\n",byte);
		case 3:
			regs2C02[addr]=byte;
			break;
		case 4:
			sprRam[regs2C02[3]]=byte;
			regs2C02[3]++;
			break;
		case 7:
			{
			//vram write
			uint16_t ppuAddr=(PPU_FVc<<12)|(PPU_Vc<<11)|(PPU_Hc<<10)|(PPU_VTc<<5)|PPU_HTc;
			PPUSetByte(ppuAddr,byte);
			if (regs2C02[0]&0x04)
			{
				ppuAddr+=32;
			}
			else
			{
				ppuAddr+=1;
			}
			PPU_FVc=(ppuAddr&0x7000)>>12;
			PPU_Vc=(ppuAddr&0x0800)>>11;
			PPU_Hc=(ppuAddr&0x0400)>>10;
			PPU_VTc=(ppuAddr&0x03E0)>>5;
			PPU_HTc=(ppuAddr&0x001F);
/*			if (regs2C02[0]&0x04)
				ppuAddr+=32;
			else
				ppuAddr++;*/
			}
			break;
		case 5:
			if (regs2C02[addr])
			{
				//second write
				PPU_FV=(byte&0x7);
				PPU_VT=(byte&0xF8)>>3;
				regs2C02[addr]=0;
			}
			else
			{
				PPU_HT=(byte&0xF8)>>3;
				PPU_FH=(byte&0x07);
				regs2C02[addr]=1;
			}
			break;
		case 6:
			if (regs2C02[addr])
			{
				//second write
				PPU_VT&=0x18;
				PPU_VT|=(byte&0xE0)>>5;
				PPU_HT=(byte&0x1F);

				PPU_FVc=PPU_FV;
				PPU_Vc=PPU_V;
				PPU_Hc=PPU_H;
				PPU_VTc=PPU_VT;
				PPU_HTc=PPU_HT;

//				ppuAddr&=0xFF00;
//				ppuAddr|=byte;
				regs2C02[addr]=0;
			}
			else
			{
				//first write
				PPU_FV=(byte&0x30)>>4;
				PPU_V=(byte&0x08)>>3;
				PPU_H=(byte&0x04)>>2;
				PPU_VT&=0x07;
				PPU_VT|=(byte&0x3)<<3;
//				ppuAddr&=0x00FF;
//				ppuAddr|=(byte&0x3F)<<8;
				regs2C02[addr]=1;
			}
			break;
	}
	return;
}
int KeyDown(int key);
void ClearKey(int key);

uint8_t controllerData=0;

uint8_t GetByte(uint16_t addr)
{
	if (addr<0x2000)
	{
		return internalRam[addr&0x7FF];
	}
	if (addr<0x4000)
	{
		return IORead(addr&0x7);
	}
	if (addr<0x8000)
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
	if (addr<0xC000)
	{
		return prgRom[addr-0x8000];
	}
	if (prgRomSize<=16384)
	{
		return prgRom[addr-0xC000];
	}

	return prgRom[addr-0x8000];
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
		IOWrite(addr&0x7,byte);
		return;
	}
	if (addr<0x8000)
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
//	printf("Unmapped Write : %08X\n",addr);
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

int g_instructionStep=0;
int g_traceStep=0;

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	(256+24)

#define HEIGHT	(262)
#define	WIDTH	(341)

#define NTSC_WIDTH	(NTSC_SAMPLES_PER_LINE)
#define NTSC_HEIGHT	(NTSC_LINES_PER_FRAME)

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define NTSC_WINDOW		1
#define REGISTER_WINDOW		2

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

void UpdateJoy()
{
}

void UpdateHardware()
{
	UpdateJoy();
}

int dumpNTSC=0;

int MasterClock=0;

FILE* ntsc_file=NULL;

void WriteNTSC(int colourClock);
void GenerateNTSC(int colourClock);

int NTSCClock;

int main(int argc,char**argv)
{
	int w,h;
	double	atStart,now,remain;
	uint16_t bp;
	
	if (argc==2)
	{
		printf("Loading %s\n",argv[1]);
		LoadCart(argv[1]);
	}
	else
	{
		printf("Currently require a rom name as argument (iNes format)\n");
		exit(-1);
	}


	/// Initialize GLFW 
	glfwInit(); 

	// Open registers OpenGL window 
	if( !(windows[REGISTER_WINDOW]=glfwOpenWindow( REGISTER_WIDTH, REGISTER_HEIGHT, GLFW_WINDOWED,"Main CPU",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[REGISTER_WINDOW],300,0);

	glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
	setupGL(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);
	
	if( !(windows[NTSC_WINDOW]=glfwOpenWindow( NTSC_WIDTH, NTSC_HEIGHT, GLFW_WINDOWED,"NTSC",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[NTSC_WINDOW],700,300);

	glfwMakeContextCurrent(windows[NTSC_WINDOW]);
	setupGL(NTSC_WINDOW,NTSC_WIDTH,NTSC_HEIGHT);

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
	
	ntscDecodeInit((uint32_t*)videoMemory[NTSC_WINDOW]);

	atStart=glfwGetTime();
	//////////////////
	AudioInitialise();

	if (InitialiseMemory())
		return -1;

	MAIN_PinSetPIN_SO(1);
	MAIN_PinSetPIN__IRQ(1);
	MAIN_PinSetPIN__NMI(1);

	MAIN_PC=GetByte(0xFFFC);
	MAIN_PC|=GetByte(0xFFFD)<<8;

// TODO PPU RESET + PROPER CPU RESET

	PPU_PinSetPIN__RST(0);
	PPU_PinSetPIN__RST(1);
	PPU_PinSetPIN__DBE(1);

	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
	{
		static uint16_t lastPC;
		uint16_t addr; 
		
		if ((!stopTheClock) || g_traceStep || g_instructionStep)
		{
			// Tick Hardware based on MASTER clocks (or as close as damn it)

			int cpuClock=MasterClock%24;
			int ppuClock=MasterClock%8;
			int ColourClock=MasterClock%12;
			NTSCClock=MasterClock%3;

			if (cpuClock==0)
			{
				MAIN_PinSetPIN_O0(1);
				addr = MAIN_PinGetPIN_AB();

				if (MAIN_DEBUG_SYNC)
				{
					lastPC=addr;
	
					if (isBreakpoint(0,lastPC))
					{
						stopTheClock=1;
					}
				}
	
				// Phase is 1 (LS139 decode gives us DBE low when Phase 1, Address=001x xxxx xxxx xxxx
#if USE_EDL_PPU
				if (1 && ((addr&0x8000)==0) && ((addr&0x4000)==0) && ((addr&0x2000)==0x2000))
				{
					PPU_PinSetPIN_RS(addr&0x7);
					PPU_PinSetPIN_RW(MAIN_PinGetPIN_RW());
					PPU_PinSetPIN_D(MAIN_PinGetPIN_DB());

					PPU_PinSetPIN__DBE(0);
					if (MAIN_PinGetPIN_RW())
					{
						MAIN_PinSetPIN_DB(PPU_PinGetPIN_D());
					}
				}
				else
#endif
				{
					if (MAIN_PinGetPIN_RW())
					{
						uint8_t  data = GetByte(addr);
						MAIN_PinSetPIN_DB(data);
					}
					if (!MAIN_PinGetPIN_RW())
					{
						SetByte(addr,MAIN_PinGetPIN_DB());
					}
				}

				PPU_PinSetPIN__DBE(1);

				UpdateHardware();

				MAIN_PinSetPIN__IRQ(1);//VIA1_PinGetPIN__IRQ());
/*			if (regs2C02[0]&0x80)
				stopTheClock=1;*/
//			printf("NMI Val : %d\n",(~(regs2C02[0]&regs2C02[2])>>7)&0x01);
#if USE_EDL_PPU
				MAIN_PinSetPIN__NMI(PPU_PinGetPIN__INT());
#else
				MAIN_PinSetPIN__NMI((~(regs2C02[0]&regs2C02[2])>>7)&0x01);
#endif
	//			printf("PPU REGS : CONTROL %02X,MASK %02X,STATUS %02X\n",PPU_Control,PPU_Mask,PPU_Status);
				MAIN_PinSetPIN_O0(0);


			}
			if (ppuClock==0)
			{
#if USE_EDL_PPU
				static uint8_t vramLowAddressLatch=0;

				PPU_PinSetPIN_CLK(1);
				if (PPU_PinGetPIN_ALE()&1)
				{
					vramLowAddressLatch=PPU_PinGetPIN_AD();
				}
				if ((PPU_PinGetPIN__RE()&1)==0)
				{
					uint16_t addr = ((PPU_PinGetPIN_PA()&0x3F)<<8)|vramLowAddressLatch;
					PPU_PinSetPIN_AD(PPUGetByte(addr));
				}
				PPU_PinSetPIN_CLK(0);
#else
#endif
				Tick2C02();
			}
//			if (ntsc_file /*&& (NTSCClock==0)*/)
			{
				GenerateNTSC(ColourClock);
				if (NTSCClock==2)
				{
					WriteNTSC(ColourClock);
				}
			}
			MasterClock+=1;
		}

		if (MasterClock>=341*262*2*4 || stopTheClock)
		{
			static int twofields=0;
			static int normalSpeed=1;

			if (twofields&1)
				ntscDecodeTick();
			if (MasterClock>=341*262*2*4)
			{
				twofields++;
				MasterClock-=341*262*2*4;
			}

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
            		
			glfwMakeContextCurrent(windows[NTSC_WINDOW]);
			ShowScreen(NTSC_WINDOW,NTSC_WIDTH,NTSC_HEIGHT);
			glfwSwapBuffers();
				
			glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
			DrawRegisterMain(videoMemory[REGISTER_WINDOW],REGISTER_WIDTH,lastPC,GetByte);
			UpdateRegisterMain(windows[REGISTER_WINDOW]);
			ShowScreen(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);
			glfwSwapBuffers();
			
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

uint16_t curLine=0;
uint16_t curClock=0;

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
uint8_t nesColoursBW[0x40];

void YFROMRGB()
{
	int a;
	for (a=0;a<0x40;a++)
	{
		uint8_t r=nesColours[a]>>16;
		uint8_t g=nesColours[a]>>8;
		uint8_t b=nesColours[a];

		nesColoursBW[a]=r*0.229f  +  g*0.587f   + b*0.114f;
	}
}

uint8_t tileData1_temp;
uint16_t tileData1_latch;
uint16_t tileData2_latch;
uint16_t attr1_latch;
uint16_t attr2_latch;

uint8_t attr1_temp;
uint8_t attr2_temp;
//uint8_t attr_latch;
//uint8_t attr_latch_1;
uint8_t tile_latch;

uint8_t flipBits(uint8_t a)
{
	int c;
	uint8_t b=0;

	for (c=0;c<8;c++)
	{
		b<<=1;
		b|=a&1;
		a>>=1;
	}

	return b;
}
		

void FetchBGData(int y,int x)
{
	switch (curClock&7)
	{
		case 0:
			break;		// Will need changing when half address bus is used
		case 1:
			// Get Name table byte
			{
				uint16_t nameTableAddress=0x2000;

				nameTableAddress|=PPU_Vc<<11;
				nameTableAddress|=PPU_Hc<<10;
				nameTableAddress|=PPU_VTc<<5;
				nameTableAddress|=PPU_HTc;
//					printf("Address To Fetch From %04X\n",nameTableAddress);
				uint8_t tile = PPUGetByte(nameTableAddress);
				tile_latch=tile;
			}
			break;
		case 2:
			break;
		case 3:
			{
				uint16_t attrAddress=0x23C0;
				attrAddress|=PPU_Vc<<11;
				attrAddress|=PPU_Hc<<10;
				attrAddress|=(PPU_VTc&0x1C)<<1;
				attrAddress|=(PPU_HTc&0x1C)>>2;
//					printf("Address To Fetch From %04X\n",attrAddress);
				attr1_temp = PPUGetByte(attrAddress);
					
				uint8_t attrshift=(PPU_VTc&0x02)<<1;
				attrshift|=PPU_HTc&0x02;

				attr1_temp>>=attrshift;
				attr1_temp&=3;

				attr1_temp|=attr1_temp<<2;
				attr1_temp|=attr1_temp<<4;

				attr2_temp=attr1_temp&0xAA;
				attr2_temp|=attr2_temp>>1;

				attr1_temp=attr1_temp&0x55;
				attr1_temp|=attr1_temp<<1;
/*
				{
					printf("Attr : %02X,%02X\n",attr1_temp,attr2_temp);
				}
*/
			}
			break;
		case 4:
			break;
		case 5:
			{
				uint16_t patternTable=0x0000;
				patternTable|=PPU_S<<12;
				patternTable|=tile_latch<<4;
				patternTable|=PPU_FVc&7;
//					printf("Address To Fetch From %04X\n",patternTable);
				tileData1_temp=flipBits(PPUGetByte(patternTable));
			}
			break;
		case 6:
			break;
		case 7:
			{
				uint16_t patternTable=0x0008;
				patternTable|=PPU_S<<12;
				patternTable|=tile_latch<<4;
				patternTable|=PPU_FVc&7;
//					printf("Address To Fetch From %04X\n",patternTable);
				tileData2_latch&=0x00FF;
				tileData2_latch|=flipBits(PPUGetByte(patternTable))<<8;
				tileData1_latch&=0x00FF;
				tileData1_latch|=tileData1_temp<<8;
				attr1_latch&=0x00FF;
				attr1_latch|=attr1_temp<<8;
				attr2_latch&=0x00FF;
				attr2_latch|=attr2_temp<<8;

				uint8_t tmpH=(PPU_Hc<<5)|PPU_HTc;
				tmpH++;
				PPU_Hc=(tmpH&0x20)>>5;
				PPU_HTc=(tmpH&0x1F);
			}
			break;
	}
}

uint8_t	SP_BUF_Tile[8];
uint8_t	SP_BUF_XCoord[8];
uint8_t	SP_BUF_Attribs[8];
uint8_t SP_BUF_Yline[8];

uint8_t SP_BUF_BitMap0[8];
uint8_t SP_BUF_BitMap1[8];
uint8_t SP_BUF_XCounter[8];
uint8_t SP_BUF_RastAttribs[8];

uint8_t SP_BUF_SpriteInRange;
uint8_t SP_BUF_CurSprite;
uint8_t SP_BUF_ZeroInRange;

uint8_t SP_BUF_RastZeroInRange;

void SpriteCompute(uint8_t clock,uint16_t cLine)
{
	uint8_t spNum=clock>>2;

	uint8_t spPhase=clock&3;
	uint16_t spSize=8;			// TODO - Fix for 16 high

	switch (spPhase)
	{
	case 0:
		{
			uint16_t compare=cLine-(sprRam[spNum*4+0]+1);
	
			SP_BUF_SpriteInRange=compare<spSize;
			SP_BUF_ZeroInRange|=SP_BUF_SpriteInRange && (spNum==0);
			if (SP_BUF_CurSprite<8)
			{
				SP_BUF_Yline[SP_BUF_CurSprite]=compare;
			}
			else
			{
//				if (SP_BUF_SpriteInRange)
//					printf("Overflow %d\n",cLine);
				SP_BUF_SpriteInRange=0;
			}
		}
		break;
	case 1:
		if (SP_BUF_SpriteInRange)
		{
			SP_BUF_Tile[SP_BUF_CurSprite]=sprRam[spNum*4+1];
			SP_BUF_Attribs[SP_BUF_CurSprite]=sprRam[spNum*4+2];
		}
		break;
	case 2:
		if (SP_BUF_SpriteInRange)
		{
			if (SP_BUF_Attribs[SP_BUF_CurSprite]&0x80)
			{
				SP_BUF_Yline[SP_BUF_CurSprite]=(~SP_BUF_Yline[SP_BUF_CurSprite])&0x07;
			}
		}
		break;
	case 3:
		if (SP_BUF_SpriteInRange)
		{
			SP_BUF_XCoord[SP_BUF_CurSprite]=sprRam[spNum*4+3];
			SP_BUF_CurSprite++;
		}
		break;
	}
}

void SpriteFetch(uint16_t curClock)
{
	curClock-=256;
	uint8_t curFetch=curClock>>3;
	switch(curClock&7)
	{
		case 0:
			break;
		case 1:
			SP_BUF_XCounter[curFetch]=SP_BUF_XCoord[curFetch];
			//dummy fetch
			break;
		case 2:
			break;
		case 3:
			SP_BUF_RastAttribs[curFetch]=SP_BUF_Attribs[curFetch];
			//dummy fetch
			break;
		case 4:
			break;
		case 5:
			if (curFetch<SP_BUF_CurSprite)
			{
				uint16_t tilePos=(SP_BUF_Tile[curFetch]<<4)+SP_BUF_Yline[curFetch];
				if (regs2C02[0]&0x08)
				{
					tilePos+=0x1000;
				}
//			printf("Tile %02X : Addr %04X\n",SP_BUF_Tile[curFetch],tilePos);
				SP_BUF_BitMap0[curFetch]=/*0xFF;*/PPUGetByte(tilePos);
			}
			else
			{
				SP_BUF_BitMap0[curFetch]=0;
			}
			break;
		case 6:
			break;
		case 7:
			if (curFetch<SP_BUF_CurSprite)
			{
				uint16_t tilePos=(SP_BUF_Tile[curFetch]<<4)+SP_BUF_Yline[curFetch]+8;
				if (regs2C02[0]&0x08)
				{
					tilePos+=0x1000;
				}
				SP_BUF_BitMap1[curFetch]=/*0xFF;*/PPUGetByte(tilePos);
			}
			else
			{
				SP_BUF_BitMap1[curFetch]=0;
			}
			break;

	}
}

uint8_t spColour;
uint8_t spZero;
uint8_t spBack;

void SpriteRender(uint8_t curClock)
{
	int a;

	spColour=0;
	for (a=0;a<8;a++)
	{
		if (SP_BUF_XCounter[a]==0)
		{
			uint8_t col0,col1,col23;
			//calculate sprite data
			if (SP_BUF_RastAttribs[a]&0x40)
			{
				col0=(SP_BUF_BitMap0[a]&0x01);
				col1=(SP_BUF_BitMap1[a]&0x01)<<1;
				col23=(SP_BUF_RastAttribs[a]&0x03)<<2;

				SP_BUF_BitMap0[a]>>=1;
				SP_BUF_BitMap1[a]>>=1;
			}
			else
			{
				col0=(SP_BUF_BitMap0[a]&0x80)>>7;
				col1=(SP_BUF_BitMap1[a]&0x80)>>6;
				col23=(SP_BUF_RastAttribs[a]&0x03)<<2;

				SP_BUF_BitMap0[a]<<=1;
				SP_BUF_BitMap1[a]<<=1;
			}
			if ((col0|col1)&&(spColour==0) && (curClock>7||regs2C02[1]&0x04))
			{
				spColour=0x10|col0|col1|col23;
				spZero=SP_BUF_RastZeroInRange && (a==0);
				spBack=SP_BUF_RastAttribs[a]&0x20;
			}
		}
		else
		{
			SP_BUF_XCounter[a]--;
		}
	}
}

uint8_t lastPixelValue;

int dumpStart=0;

static int offsoffs=0;
	
//static int wave[4]={0,20,0,-20};
//-----888888-

static int colourBurstWave0[12]={0,0,0,0,0,0,0,0,0,0,0,0};
static int colourBurstWave1[12]={1,1,1,1,1,1,-1,-1,-1,-1,-1,-1};
static int colourBurstWave2[12]={1,1,1,1,1,-1,-1,-1,-1,-1,-1,1};
static int colourBurstWave3[12]={1,1,1,1,-1,-1,-1,-1,-1,-1,1,1};
static int colourBurstWave4[12]={1,1,1,-1,-1,-1,-1,-1,-1,1,1,1};
static int colourBurstWave5[12]={1,1,-1,-1,-1,-1,-1,-1,1,1,1,1};
static int colourBurstWave6[12]={1,-1,-1,-1,-1,-1,-1,1,1,1,1,1};
static int colourBurstWave7[12]={-1,-1,-1,-1,-1,-1,1,1,1,1,1,1};
static int colourBurstWave8[12]={-1,-1,-1,-1,-1,1,1,1,1,1,1,-1};
static int colourBurstWave9[12]={-1,-1,-1,-1,1,1,1,1,1,1,-1,-1};
static int colourBurstWaveA[12]={-1,-1,-1,1,1,1,1,1,1,-1,-1,-1};
static int colourBurstWaveB[12]={-1,-1,1,1,1,1,1,1,-1,-1,-1,-1};
static int colourBurstWaveC[12]={-1,1,1,1,1,1,1,-1,-1,-1,-1,-1};

//static int waveGuide={-4,-3,-1,1,3,4,3,1,-1,-3};

//static int TopBottomPercentage={0,100,0,100,0,100,0,100,0,100,0,100,0,100,0,100}
//static int TopBottomPercentage[12]={64,80,96,128,96,80,64,32,16,0,16,32};
static int TopBottomPercentage[12]={64,104,128,128,128,104,64,24,0,0,0,24};

static int cClock=0;

void DumpNTSCComposite(uint8_t level,uint8_t col,uint8_t low,uint8_t high)
{
	static int _3rds=0;
	int actualLevel=((cClock+col+level)%12)<6?low:high;
//	int actualLevel=low;

	cClock+=8;

	fwrite(&actualLevel,1,1,ntsc_file);
	fwrite(&actualLevel,1,1,ntsc_file);

	_3rds+=2;
	if (_3rds>=3)
	{
		//actualLevel=((cClock+col+level)%12)<6?low:high;
		_3rds-=3;
		fwrite(&actualLevel,1,1,ntsc_file);
	}

}
uint8_t activeNTSCSignalLow;
uint8_t activeNTSCSignalHi;
uint8_t activeNTSCDisplay=1;
uint8_t activeNTSCSignalCol;

uint8_t activeNTSCBase;
uint8_t	activeNTSCAmplitude;
uint8_t activeNTSCPhase;

#define SIGNAL_OFFSET	(60)
#define SIGNAL_RANGE	(130)

uint16_t avgNTSC[3];

void GenerateNTSC(int colourClock)
{
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

	avgNTSC[NTSCClock]=actualLevel;
}

void WriteNTSC(int colourClock)
{
	uint8_t actualLevel=(avgNTSC[0]+avgNTSC[1]+avgNTSC[2])/3;
	ntscDecodeAddSample(actualLevel);
//	fwrite(&actualLevel,1,1,ntsc_file);
}

void PPU_SetVideo(uint8_t x,uint8_t y,uint8_t col)
{
	uint32_t* outputTexture = (uint32_t*)videoMemory[MAIN_WINDOW];


//	printf("%02X,%02X  %02X\n",x,y,col&0xF);
	//if (x>=8)
	{
		if (col&3)
		{
			outputTexture[y*WIDTH+x]=nesColours[palIndex[col&0x1F]];
			lastPixelValue=palIndex[col];
	//		outputTexture[y*WIDTH+x]=0xFFFFFFFF;//nesColours[palIndex[col&0x1F]];
		}
		else
		{
			outputTexture[y*WIDTH+x]=nesColours[palIndex[0]];
			lastPixelValue=palIndex[0];
	//		outputTexture[y*WIDTH+x]=0;//nesColours[palIndex[0]];
		}
	}
}

void Tick2C02()
{
	static int triCnt=0;
#if !USE_EDL_PPU
	static int field=0;
	uint32_t* outputTexture = (uint32_t*)videoMemory[MAIN_WINDOW];
	int a;

	static uint16_t lastCollision=0;

	static firstEver=1;
	if (firstEver)
	{
		YFROMRGB();
		firstEver=0;
	}


/*	triCnt++;
	if (triCnt==2)
		triCnt=0;*/

	if (curLine<20)
	{
		if (curLine==0 && curClock==0)
		{
			offsoffs++;
			if (!dumpStart)
			{
				dumpStart=dumpNTSC;
				if (dumpStart)
				{
					triCnt=2;//*5000;
					ntsc_file=fopen("out.ntsc","wb+");
				}
				else
				{
					ntsc_file=NULL;
				}
				dumpNTSC=0;
			}
		}
		// IDLE PPU
	}
	else
	{
		if (curLine==20)
		{
			if (curClock<256 && (regs2C02[1]&0x10))
			{
				SpriteCompute(curClock,curLine-20);
			}

			if (curClock==0)
			{
				regs2C02[2]&=0xBF;
			}
			if (curClock==256 && regs2C02[1]&0x08)
			{
				PPU_FVc=PPU_FV;
				PPU_Vc=PPU_V;
				PPU_Hc=PPU_H;
				PPU_VTc=PPU_VT;
				PPU_HTc=PPU_HT;
			}
			// DUMMY line
			if ((curClock<256) && (regs2C02[1]&0x08))
			{
				FetchBGData(20,curClock+16);
			}

			if ((curClock>=320) && (curClock<=335) && (regs2C02[1]&0x08))
			{
				FetchBGData(20,curClock-320);
				tileData1_latch>>=8;
				tileData2_latch>>=8;
				attr1_latch>>=8;
				attr2_latch>>=8;
			}
			/*
			if (curClock==340)
				field++;
			if ((curClock==340)&&(field&1))
				curClock++;*/
		}
		else
		{
			if (curLine<261)
			{
				if (curClock<256 && (regs2C02[1]&0x10))
				{
					SpriteCompute(curClock,curLine-20);
					SpriteRender(curClock);
				}

				if ((curClock<256) && (regs2C02[1]&0x08) && ((curClock>7) || (regs2C02[1]&0x02)))
				{
					uint8_t shift=PPU_FHl;

					uint16_t fetchMask=0x0001;
					fetchMask<<=shift;

					uint8_t col1 = fetchMask & tileData1_latch;
					uint8_t col2 = fetchMask & tileData2_latch;
					uint8_t col3 = fetchMask & attr1_latch;
					uint8_t col4 = fetchMask & attr2_latch;
					col1>>=shift;
					col2>>=shift;
					col3>>=shift;
					col4>>=shift;
					
					col1<<=0;
					col2<<=1;
					col3<<=2;
					col4<<=3;

					tileData1_latch>>=1;
					tileData2_latch>>=1;
					attr1_latch>>=1;
					attr2_latch>>=1;

					uint8_t colbpl = col1|col2;

					uint8_t attrshift=(PPU_VTc&0x02)<<1;
					attrshift|=PPU_HTc&0x02;

					uint8_t col=colbpl|col3|col4;
					if (colbpl==0)
					{
						lastPixelValue=palIndex[spColour];
						outputTexture[(curLine-21)*WIDTH+curClock]=nesColours[palIndex[spColour]];
					}
					else
					{
						//printf("MM %02X\n",col&0xF);
						if (spZero && lastCollision==0 && spColour)
						{
							lastCollision=1;
							regs2C02[2]|=0x40;
						}
						if (!spBack && spColour)
						{
							lastPixelValue=palIndex[spColour];
							outputTexture[(curLine-21)*WIDTH + curClock]=nesColours[palIndex[spColour]];
						}
						else
						{
							lastPixelValue=palIndex[col];
							outputTexture[(curLine-21)*WIDTH + curClock]=nesColours[palIndex[col]];
						}
					}
					//spColour=0;
				}
				if ((curClock<256) && ((regs2C02[1]&0x08)==0))
				{
					uint16_t ppuAddr=(PPU_FVc<<12)|(PPU_Vc<<11)|(PPU_Hc<<10)|(PPU_VTc<<5)|PPU_HTc;
					ppuAddr&=0x3FFF;
					if (ppuAddr>0x3EFF)
					{
						lastPixelValue=palIndex[ppuAddr&0x1F];
						outputTexture[(curLine-21)*WIDTH + curClock]=nesColours[lastPixelValue];
					}
				}

				if ((curClock<256) && (regs2C02[1]&0x08))
				{
					FetchBGData(curLine-21,curClock+16);
				}
				if ((curClock>=256) && (curClock<=319) && (regs2C02[1]&0x10))
				{
					SpriteFetch(curClock);
				}
				if ((curClock>=320) && (curClock<=335) && (regs2C02[1]&0x08))
				{
					FetchBGData(curLine-20,curClock-320);
					if (curClock==328)
					{
						tileData1_latch>>=8;
						tileData2_latch>>=8;
						attr1_latch>>=8;
						attr2_latch>>=8;
					}
				}

			}
			else
			{
				// IDLE PPU
			}
		}
	}
#endif

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


//			lastPixelValue=(curClock>>4)|((curLine-21)&0x30);

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
	}

//	cClock+=8;		// gives 12 phase stepping for colour clock (which results in 21.47/6)
	curClock++;
	if ((curClock==320) && (regs2C02[1]&0x08))
	{
#if !USE_EDL_PPU
		//pretend this is the horizontal blanking pulse?
		uint16_t tmpV=(PPU_Vc<<8)|(PPU_VTc<<3)|PPU_FVc;

		if (curLine>=21)
		{
			// Check for VT overflow this operation
			if (PPU_FVc==7 && PPU_VTc==29)
			{
				tmpV+=8+8;
			}
			tmpV++;
		}
		PPU_Vc=(tmpV&0x100)>>8;
		PPU_VTc=(tmpV&0xF8)>>3;
		PPU_FVc=(tmpV&0x07);

		// RELOAD here apparantly
		PPU_Hc=PPU_H;
		PPU_HTc=PPU_HT;
		PPU_FHl=PPU_FH;
#endif
	}
	if (curClock>=341)
	{
		curClock=0;
		curLine++;
#if !USE_EDL_PPU
		lastCollision=0;
		SP_BUF_CurSprite=0;
		SP_BUF_RastZeroInRange=SP_BUF_ZeroInRange;
		SP_BUF_ZeroInRange=0;
		if (curLine==20)
		{
			regs2C02[2]&=0x7F;
		}
#endif
		if (curLine>=262)
		{
			if (ntsc_file)
			{
				triCnt--;
				if (triCnt==0)
				{
					fclose(ntsc_file);
					dumpStart=0;
				}
			}
			curLine=0;
			regs2C02[2]|=0x80;
		}
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

int16_t currentDAC[4] = {0,0,0,0};

void _AudioAddData(int channel,int16_t dacValue)
{
	currentDAC[channel]=dacValue;
}

uint32_t tickCnt=0;
uint32_t tickRate=((22152*4096)/(44100/50));

/* audio ticked at same clock as everything else... so need a step down */
void UpdateAudio()
{
	tickCnt+=1*4096;
	
	if (tickCnt>=tickRate*50)
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

		printf("prgSize : %08X\n",prgSize*16384);
		printf("chrSize : %08X\n",chrSize*8192);
		printf("flags6 : %02X\n",flags6);
		printf("flags7 : %02X\n",flags7);
		printf("prgRamSize : %02X\n",prgRamSize);
		printf("flags9 : %02X\n",flags9);
		printf("flags10 : %02X\n",flags10);
		
		if ((flags6&0xF0)!=0 || (flags7&0xF0)!=0)
		{
			printf("Only supports mapper 0!\n");
			exit(-1);
		}
		if ((flags6&0x0E)!=0)
		{
			printf("No Bat/Train/4Scr Support\n");
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
	}
}

///////////////////////////////////////////

// Testing Bed for PPU on a chip

void PPU_SetByte(uint16_t addr,uint8_t value)
{
	PPUSetByte(addr,value);
	//printf("Store To : %04X (%02X)\n",addr,value);	
}

uint8_t PPU_GetByte(uint16_t addr)
{
	//printf("Fetch From : %04X\n",addr);
	return PPUGetByte(addr);
}
