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

void AudioKill();
void AudioInitialise();
void UpdateAudio();
void _AudioAddData(int channel,int16_t dacValue);

uint16_t MAIN_PinGetPIN_AB();
uint8_t MAIN_PinGetPIN_DB();
void MAIN_PinSetPIN_DB(uint8_t);
void MAIN_PinSetPIN_O0(uint8_t);
void MAIN_PinSetPIN_SO(uint8_t);
uint8_t MAIN_PinGetPIN_SYNC();
uint8_t MAIN_PinGetPIN_RW();
void MAIN_PinSetPIN__IRQ(uint8_t);
void MAIN_PinSetPIN__RES(uint8_t);

uint8_t *prgRom;
uint32_t prgRomSize;
uint8_t *chrRom;
uint32_t chrRomSize;

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
uint16_t ppuAddr;

uint8_t xScroll;
uint8_t yScroll;

uint8_t palIndex[0x20];

uint8_t PPUGetByte(uint16_t addr)
{
	addr&=0x3FFF;
	if (addr<0x2000)
	{
		return chrRom[addr&0x1FFF];		// WILL NEED FIXING
	}
	if (addr<0x3F00)
	{
		return ppuRam[(addr-0x2000)&0x7FF];	// WILL NEED FIXING
	}
	return palIndex[addr&0x1F];/// PALLETTE
}
void PPUSetByte(uint16_t addr,uint8_t byte)
{
	addr&=0x3FFF;
	if (addr<0x2000)
		return;		/// assuming fixed tile ram
	if (addr<0x3F00)
	{
		ppuRam[(addr-0x2000)&0x7FF]=byte;
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
	switch (addr)
	{
		case 0:
		printf("IO Read : %d\n",addr&0x7);
			break;
		case 1:
		printf("IO Read : %d\n",addr&0x7);
			break;
		case 2:
			regs2C02[addr]=0;
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
			regs2C02[addr]=PPUGetByte(ppuAddr);
			if ((ppuAddr&0x3FFF)>0x3eFF)
			{
				value=regs2C02[addr];
			}
			if (regs2C02[0]&0x04)
				ppuAddr+=32;
			else
				ppuAddr++;
			break;
	}
	return value;
}

void IOWrite(uint8_t addr,uint8_t byte)
{
	switch (addr)
	{
		case 0:
			printf("IO Write : %d<-%02X\n",addr&0x7,byte);
		case 1:
		case 3:
			regs2C02[addr]=byte;
			break;
		case 4:
			sprRam[regs2C02[3]]=byte;
			regs2C02[3]++;
			break;
		case 7:
			//vram write
			PPUSetByte(ppuAddr,byte);
			if (regs2C02[0]&0x04)
				ppuAddr+=32;
			else
				ppuAddr++;
			break;
		case 5:
			if (regs2C02[addr])
			{
				//second write
				yScroll=byte;
				regs2C02[addr]=0;
			}
			else
			{
				xScroll=byte;
				regs2C02[addr]=1;
			}
			break;
		case 6:
			if (regs2C02[addr])
			{
				//second write
				ppuAddr&=0xFF00;
				ppuAddr|=byte;
				regs2C02[addr]=0;
			}
			else
			{
				//first write
				ppuAddr&=0x00FF;
				ppuAddr|=(byte&0x3F)<<8;
				regs2C02[addr]=1;
			}
			break;
	}
	return;
}

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
		if (addr>=0x4016 && addr<=0x4017)	//controller ports
			return 0;
		printf("Unkown Read : %08X\n",addr);
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
		if (addr==0x4014)
		{
			int a;				///DMA HACK
			for (a=0;a<256;a++)
			{
				IOWrite(4,GetByte((byte<<8)+a));
			}
			return;

		}
		if (addr>=0x4015 && addr<=0x4017)		// sound channel + controllers.. todo
			return;
		printf("Unknown Write : %08X\n",addr);
		return;
	}
	printf("Unmapped Write : %08X\n",addr);
	return;
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

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
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
		

int main(int argc,char**argv)
{
	int w,h;
	double	atStart,now,remain;
	uint16_t bp;

	/// Initialize GLFW 
	glfwInit(); 

	// Open registers OpenGL window 
	if( !(windows[REGISTER_WINDOW]=glfwOpenWindow( REGISTER_WIDTH, REGISTER_HEIGHT, GLFW_WINDOWED,"Main CPU",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[REGISTER_WINDOW],700,300);

	glfwMakeContextCurrent(windows[REGISTER_WINDOW]);
	setupGL(REGISTER_WINDOW,REGISTER_WIDTH,REGISTER_HEIGHT);
	
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

	atStart=glfwGetTime();
	//////////////////
	AudioInitialise();

	if (InitialiseMemory())
		return -1;

	if (argc==2)
	{
		LoadCart(argv[1]);
	}

	MAIN_PinSetPIN_SO(1);
	MAIN_PinSetPIN__IRQ(1);
	MAIN_PinSetPIN__NMI(1);

	MAIN_PC=GetByte(0xFFFC);
	MAIN_PC|=GetByte(0xFFFD)<<8;

	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
	{
		static uint16_t lastPC;
		uint16_t addr; 
		
		if ((!stopTheClock) || g_traceStep || g_instructionStep)
		{
			MAIN_PinSetPIN_O0(1);
			addr = MAIN_PinGetPIN_AB();

			if (MAIN_PinGetPIN_SYNC())
			{
				lastPC=addr;

				if (isBreakpoint(0,lastPC))
				{
					stopTheClock=1;
				}
			}

			if (MAIN_PinGetPIN_RW())
			{
				uint8_t  data = GetByte(addr);
				MAIN_PinSetPIN_DB(data);
			}
			if (!MAIN_PinGetPIN_RW())
			{
				SetByte(addr,MAIN_PinGetPIN_DB());
			}


			UpdateHardware();

			MAIN_PinSetPIN__IRQ(1);//VIA1_PinGetPIN__IRQ());
//			printf("NMI Val : %d\n",(~(regs2C02[0]&regs2C02[2])>>7)&0x01);
			MAIN_PinSetPIN__NMI((~(regs2C02[0]&regs2C02[2])>>7)&0x01);

			MAIN_PinSetPIN_O0(0);

			Tick2C02();
			Tick2C02();
			Tick2C02();

			pixelClock+=3;
		}

		if (pixelClock>=341*262 || stopTheClock)
		{
			static int normalSpeed=1;

			if (pixelClock>=341*262)
				pixelClock-=341*262;

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
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

uint16_t latchedScrollX;

void Tick2C02()
{
	uint32_t* outputTexture = (uint32_t*)videoMemory[MAIN_WINDOW];
	int a;

	uint8_t lastPixel=0;
	static uint16_t lastCollision=0;

	if (curLine<20)
	{
		// IDLE PPU
	}
	else
	{
		if (curLine==20)
		{
			// DUMMY line
		}
		else
		{
			if (curLine<261)
			{
				// Real Render  - for now.. cheat and draw in place

				if (curClock<256)
				{
					// Look up pixel from tile info

					uint8_t yPos=curLine-21;
					uint8_t xPos=curClock;

					uint16_t vX=(xPos+latchedScrollX)&0x1FF;

					uint16_t namePos = (yPos/8)*32 + ((vX&0xFF)/8);

					uint8_t tile = PPUGetByte(namePos+0x2000);
					if (vX>255)
					{
						tile = PPUGetByte(namePos+0x2400);
					}

					uint16_t tilePos = tile*16 + (yPos&7);

					tilePos+=0x1000;
					uint8_t tileData1=PPUGetByte(tilePos);
					uint8_t tileData2=PPUGetByte(tilePos+8);

					uint16_t fetchMask=0x80;
					fetchMask>>=xPos&7;

					uint8_t col1 = fetchMask & tileData1;
					uint8_t col2 = fetchMask & tileData2;
					col1>>=(7-(xPos&7));
					col2>>=(7-(xPos&7));
					
					col1<<=0;
					col2<<=1;

					uint8_t col = col1|col2;

					uint16_t attrPos = (yPos/32)*8 + (xPos/32);

					uint8_t attr = PPUGetByte(0x2000+0x3C0+attrPos);

					uint8_t attrX=(xPos&0x1F)/16;
					uint8_t attrY=(yPos&0x1F)/16;

					if (col==0)
					{
						outputTexture[yPos*WIDTH+xPos]=nesColours[palIndex[0]];
						lastPixel=0;
					}
					else
					{
						lastPixel=1;
						if (attrX==0 && attrY==0)
							col|=(attr&0x03)<<2;
						if (attrX==1 && attrY==0)
							col|=(attr&0x0C);
						if (attrX==0 && attrY==1)
							col|=(attr&0x30)>>2;
						if (attrX==1 && attrY==1)
							col|=(attr&0xC0)>>4;
						outputTexture[yPos*WIDTH + xPos]=nesColours[palIndex[col]];
					}
				}

			}
			else
			{
				// IDLE PPU
			}
		}
	}

//sprite hack

	if (curLine>20 && curLine<261 && curClock<256)
	{
		uint8_t sprSize=8;
		uint16_t yPos=curLine-21;

		if (regs2C02[0]&0x20)
			sprSize=16;

		for (a=0;a<64;a++)
		{
			int16_t sY=yPos;
			sY-=sprRam[a*4+0];

			if (sY>=0 && sY<sprSize)
			{
				int16_t sX=curClock;
				sX-=sprRam[a*4+3];

				if (sprRam[a*4+2]&0x80)
					sY=7-sY;

				if (sX>=0 && sX<8)
				{
					uint8_t sprY=sY;
					uint8_t sprX=sX;

					uint8_t tile=sprRam[a*4+1];		// not for 8x16!!!
					
					uint16_t tilePos = tile*16 + sprY;

					uint8_t tileData1=PPUGetByte(tilePos);
					uint8_t tileData2=PPUGetByte(tilePos+8);

					uint16_t fetchMask=0x80;

					if (sprRam[a*4+2]&0x40)
						sprX=7-sprX;

					fetchMask>>=sprX&7;

					uint8_t col1 = fetchMask & tileData1;
					uint8_t col2 = fetchMask & tileData2;
					col1>>=(7-(sprX&7));
					col2>>=(7-(sprX&7));
					
					col1<<=0;
					col2<<=1;

					uint8_t col = col1|col2;


					if (col)
					{
						if (a==0 && lastCollision==0 && lastPixel==1)
						{
							lastCollision=1;
							regs2C02[2]|=0x40;
						}
						col|=((sprRam[a*4+2]&0x3)<<2)|0x10;
						outputTexture[yPos*WIDTH + curClock]=nesColours[palIndex[col]];
					}
				}
			}
		}
	}

	curClock++;
	if (curClock==256)
	{
		latchedScrollX = xScroll | ((regs2C02[0]&0x1)<<8);
	}
	if (curClock>=341)
	{
		curClock=0;
		curLine++;
		lastCollision=0;
		if (curLine>=262)
		{
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
			return;
		}

		prgSize=ptr[4];
		chrSize=ptr[5];
		flags6=ptr[6];
		flags7=ptr[7];
		prgRamSize=ptr[8];
		flags9=ptr[9];
		flags10=ptr[10];

		if ((flags6&0xF0)!=0 || (flags7&0xF0)!=0)
		{
			printf("Only supports mapper 0!\n");
			return;
		}

		printf("prgSize : %08X\n",prgSize*16384);
		printf("chrSize : %08X\n",chrSize*8192);
		printf("flags6 : %02X\n",flags6);
		printf("flags7 : %02X\n",flags7);
		printf("prgRamSize : %02X\n",prgRamSize);
		printf("flags9 : %02X\n",flags9);
		printf("flags10 : %02X\n",flags10);

		prgRomSize=prgSize*16384;
		chrRomSize=chrSize*8192;

		prgRom=&ptr[16];
		chrRom=&ptr[16+prgRomSize];
	}
}


