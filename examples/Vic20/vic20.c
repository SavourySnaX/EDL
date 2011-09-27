/*
 * VIC20 implementation
 *
 *
 *
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

int DISK_InitialiseMemory();
void DISK_Reset();
void DISK_Tick(uint8_t* clk,uint8_t* atn,uint8_t* data);

void AudioKill();
void AudioInitialise();
void UpdateAudio();
void _AudioAddData(int channel,int16_t dacValue);

uint16_t MAIN_PinGetPIN_AB();
uint8_t MAIN_PinGetPIN_DB();
void MAIN_PinSetPIN_DB(uint8_t);
void MAIN_PinSetPIN_O0(uint8_t);
uint8_t MAIN_PinGetPIN_SYNC();
uint8_t MAIN_PinGetPIN_RW();
void MAIN_PinSetPIN__IRQ(uint8_t);
void MAIN_PinSetPIN__RES(uint8_t);

//	VIA 1	9110-911F			VIA 2	9120-912F
//	NMI					IRQ
//	CA1 - ~RESTORE				CA1	CASSETTE READ
// 	PA0 - SERIAL CLK(IN)			PA0	ROW INPUT
//	PA1 - SERIAL DATA(IN)			PA1	ROW INPUT
//	PA2 - JOY0				PA2	ROW INPUT
//	PA3 - JOY1				PA3	ROW INPUT
//	PA4 - JOY2				PA4	ROW INPUT
//	PA5 - LIGHT PEN				PA5	ROW INPUT
//	PA6 - CASETTE SWITCH			PA6	ROW INPUT
//	PA7 - SERIAL ATN(OUT)			PA7	ROW INPUT
//	CA2 - CASETTE MOTOR			CA2	SERIAL CLK (OUT)
//
//	CB1 - USER PORT				CB1	SERIAL SRQ(IN)
//	PB0 - USER PORT				PB0	COLUMN OUTPUT
//	PB1 - USER PORT				PB1	COLUMN OUTPUT
//	PB2 - USER PORT				PB2	COLUMN OUTPUT
//	PB3 - USER PORT				PB3	COLUMN OUTPUT	RECORD
//	PB4 - USER PORT				PB4	COLUMN OUTPUT
//	PB5 - USER PORT				PB5	COLUMN OUTPUT
//	PB6 - USER PORT				PB6	COLUMN OUTPUT
//	PB7 - USER PORT				PB7	COLUMN OUTPUT  JOY3
//	CB2 - USER PORT				CB2	SERIAL DATA (OUT)
//
//
//
void	VIA0_PinSetPIN_PA(uint8_t);
uint8_t VIA0_PinGetPIN_PA();
void	VIA0_PinSetPIN_CA1(uint8_t);
void	VIA0_PinSetPIN_CA2(uint8_t);
uint8_t VIA0_PinGetPIN_CA2();
void	VIA0_PinSetPIN_PB(uint8_t);
uint8_t VIA0_PinGetPIN_PB();
void	VIA0_PinSetPIN_CB1(uint8_t);
uint8_t VIA0_PinGetPIN_CB1();
void	VIA0_PinSetPIN_CB2(uint8_t);
uint8_t VIA0_PinGetPIN_CB2();
void	VIA0_PinSetPIN_RS(uint8_t);
void	VIA0_PinSetPIN_CS(uint8_t);
void	VIA0_PinSetPIN_D(uint8_t);
uint8_t VIA0_PinGetPIN_D();
void	VIA0_PinSetPIN__RES(uint8_t);
void	VIA0_PinSetPIN_O2(uint8_t);
void	VIA0_PinSetPIN_RW(uint8_t);
uint8_t VIA0_PinGetPIN__IRQ();

void	VIA1_PinSetPIN_PA(uint8_t);
uint8_t VIA1_PinGetPIN_PA();
void	VIA1_PinSetPIN_CA1(uint8_t);
void	VIA1_PinSetPIN_CA2(uint8_t);
uint8_t VIA1_PinGetPIN_CA2();
void	VIA1_PinSetPIN_PB(uint8_t);
uint8_t VIA1_PinGetPIN_PB();
void	VIA1_PinSetPIN_CB1(uint8_t);
uint8_t VIA1_PinGetPIN_CB1();
void	VIA1_PinSetPIN_CB2(uint8_t);
uint8_t VIA1_PinGetPIN_CB2();
void	VIA1_PinSetPIN_RS(uint8_t);
void	VIA1_PinSetPIN_CS(uint8_t);
void	VIA1_PinSetPIN_D(uint8_t);
uint8_t VIA1_PinGetPIN_D();
void	VIA1_PinSetPIN__RES(uint8_t);
void	VIA1_PinSetPIN_O2(uint8_t);
void	VIA1_PinSetPIN_RW(uint8_t);
uint8_t VIA1_PinGetPIN__IRQ();

// Step 1. Memory

unsigned char D20[0x2002];
unsigned char D40[0x2002];
unsigned char D60[0x2002];
unsigned char DA0[0x2002];

unsigned char CRom[0x1000];
unsigned char BRom[0x2000];
unsigned char KRom[0x2000];

unsigned char RamLo[0x400];
unsigned char RamHi[0xE00];

unsigned char CRam[0x400];
unsigned char SRam[0x200];

#define USE_CART_A0		0
#define USE_CART_60		0
#define USE_CART_40		0
#define USE_CART_20		0
#define ALLOW_WRITE_A0		0
#define ALLOW_WRITE_60		0
#define ALLOW_WRITE_40		0
#define ALLOW_WRITE_20		0

int playDown=0;
int recDown=0;

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
	if (LoadRom(CRom,0x1000,"roms/901460-03.ud7"))
		return 1;
	if (LoadRom(BRom,0x2000,"roms/901486-01.ue11"))
		return 1;
	if (LoadRom(KRom,0x2000,"roms/901486-07.ue12"))
		return 1;

#if 1
	if (LoadRom(D20,0x2002,"roms/Donkey Kong-2000.prg"))
		return 1;
	if (LoadRom(DA0,0x2002,"roms/Donkey Kong-A000.prg"))
		return 1;
#else
//	if (LoadRom(D60,0x2002,"roms/Lode Runner.60"))
//		return 1;
//	if (LoadRom(DA0,0x2002,"roms/Lode Runner.a0"))
//		return 1;

//	if (LoadRom(DA0,0x2002,"roms/solarsystem.a0"))
//		return 1;
	if (LoadRom(DA0,0x2002,"roms/Cosmic Cruncher (1982)(Commodore).a0"))
		return 1;
//	if (LoadRom(DA0,0x2002,"roms/Omega Race (1982)(Commodore).a0"))
//		return 1;
//	if (LoadRom(DA0,0x2002,"roms/Arcadia (19xx)(-).a0"))
//		return 1;
#endif
	return 0;
}

#if 0
uint8_t VIAGetByte(int chipNo,int regNo);
void VIASetByte(int chipNo,int regNo,uint8_t byte);
void VIATick(int chipNo,uint8_t PB0);
#endif

uint8_t GetByte6561(int regNo);
void SetByte6561(int regNo,uint8_t byte);
void Tick6561();

uint8_t lastBus=0xFF;

int doDebug=0;

uint8_t GetByte(uint16_t addr)
{
	if (addr<0x0400)
	{
		return RamLo[addr];
	}
	if (addr<0x1000)
	{
		return lastBus;
	}
	if (addr<0x1E00)
	{
		return RamHi[addr-0x1000];
	}
	if (addr<0x2000)
	{
		return SRam[addr-0x1E00];
	}
	if (addr<0x4000)
	{
#if USE_CART_20
		return D20[2+(addr-0x2000)];
#else
		return lastBus;
#endif
	}
	if (addr<0x6000)
	{
#if USE_CART_20
		return D40[2+(addr-0x4000)];
#else
		return lastBus;
#endif
	}
	if (addr<0x8000)
	{
#if USE_CART_60
		return D60[2+(addr-0x6000)];
#else
		return lastBus;
#endif
	}
	if (addr<0x9000)
	{
		return CRom[addr-0x8000];
	}
	if (addr<0x9400)
	{
		if (addr>=0x9000 && addr<=0x900F)
		{
			return GetByte6561(addr&0x0F);
		}
		if (addr>=0x9110 && addr<=0x912F)
		{
			if (addr&0x0020)
			{
				if (doDebug)
				{
					printf("Getting PIN_D from VIA1\n");
				}
				return VIA1_PinGetPIN_D();		//TODO FIX.. according to the schematic via addressing is 1001 00xx xxBA RRRR
			}
			else
			{
				if (doDebug)
				{
					printf("Getting PIN_D from VIA0\n");
				}
				return VIA0_PinGetPIN_D();
			}
		}
		// Various expansions
		printf("Attempt to acccess : %04X\n",addr);
		return lastBus;
	}
	if (addr<0x9800)
	{
		return CRam[addr-0x9400]&0x0F;
	}
	if (addr<0xA000)
	{
		return lastBus;
	}
	if (addr<0xC000)
	{
#if USE_CART_A0
		return DA0[2+(addr-0xA000)];
#else
		return lastBus;
#endif
	}
	if (addr<0xE000)
	{
		return BRom[addr-0xC000];
	}
	return KRom[addr-0xE000];
}

void SetByte(uint16_t addr,uint8_t byte)
{
	if (addr<0x0400)
	{
		RamLo[addr]=byte;
		return;
	}
	if (addr<0x1000)
	{
		return;
	}
	if (addr<0x1E00)
	{
		RamHi[addr-0x1000]=byte;
		return;
	}
	if (addr<0x2000)
	{
		SRam[addr-0x1E00]=byte;
		return;
	}
	if (addr<0x4000)
	{
#if ALLOW_WRITE_20
		D20[2+(addr-0x2000)]=byte;
#endif
		return;
	}
	if (addr<0x6000)
	{
#if ALLOW_WRITE_40
		D40[2+(addr-0x4000)]=byte;
#endif
		return;
	}
	if (addr<0x8000)
	{
#if ALLOW_WRITE_60
		D60[2+(addr-0x6000)]=byte;
#endif
		return;
	}
	if (addr<0x9000)
	{
		return;
	}
	if (addr<0x9400)
	{
		if (addr>=0x9000 && addr<=0x900F)
		{
			SetByte6561(addr&0x0F,byte);
			return;
		}
		if (addr>=0x9110 && addr<=0x912F)
		{
			if (addr&0x0020)
			{
				VIA1_PinSetPIN_D(byte);
			}
			else
			{
				VIA0_PinSetPIN_D(byte);
			}
			return;
		}
		// Various expansions
		printf("WRITE Attempt to acccess :%02X %04X\n",byte,addr);
		return;
	}
	if (addr<0x9800)
	{
		CRam[addr-0x9400]=byte&0x0F;
		return;
	}
	if (addr<0xA000)
	{
		return;
	}
	if (addr<0xC000)
	{
#if ALLOW_WRITE_A0
		DA0[2+(addr-0xA000)]=byte;
#endif
		return;
	}
	if (addr<0xE000)
	{
		return;
	}
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

#if 0
struct via6522
{
	uint8_t CA1;
	uint8_t CA2;
	uint8_t CB1;
	uint8_t CB2;
	uint8_t	ORB;
	uint8_t IRB;
	uint8_t ORA;
	uint8_t IRA;
	uint8_t	DDRB;
	uint8_t DDRA;
	uint16_t	T1C;
	uint8_t T1LL;
	uint8_t T1LH;
	uint8_t	T2LL;
	uint16_t	T2C;
	uint8_t SR;
	uint8_t ACR;
	uint8_t PCR;
	uint8_t IFR;
	uint8_t IER;
	uint8_t T2TimerOff;
};

struct via6522	via[2];
#endif

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

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	256

#define TIMING_WIDTH	680
#define TIMING_HEIGHT	400

#define HEIGHT	(312-28)
#define	WIDTH	(284-(5*8))

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define TIMING_WINDOW		1
#define REGISTER_WINDOW		2

GLFWwindow windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

uint8_t CTRL_1=0;
uint8_t CTRL_2=0;
uint8_t CTRL_3=0;
uint8_t CTRL_4=0;
uint8_t CTRL_5=0;
uint8_t CTRL_6=0;
uint8_t CTRL_7=0;
uint8_t CTRL_8=0;
uint8_t CTRL_9=0;
uint8_t CTRL_10=0;
uint8_t CTRL_11=0;
uint8_t CTRL_12=0;
uint8_t CTRL_13=0;
uint8_t CTRL_14=0;
uint8_t CTRL_15=0;
uint8_t CTRL_16=0;

uint32_t	cTable[16]={
	0x00000000,0x00ffffff,0x00782922,0x0087d6dd,0x00aa5fb6,0x0055a049,0x0040318d,0x00bfce72,
	0x00aa7449,0x00eab489,0x00b86962,0x00c7ffff,0x00eaf9f6,0x0094e089,0x008071cc,0x00ffffb2,
			};

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

void sizeHandler(GLFWwindow window,int xs,int ys)
{
	glViewport(0, 0, xs, ys);
}

void LoadTape(const char* fileName);
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
	uint8_t column=~VIA1_PinGetPIN_PB();

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

	VIA1_PinSetPIN_PA(keyval);
}

void UpdateJoy()
{
	uint8_t joyVal=0xFF;
	joyVal&=KeyDown(GLFW_KEY_KP_8)?0xFB:0xFF;
	joyVal&=KeyDown(GLFW_KEY_KP_2)?0xF7:0xFF;
	joyVal&=KeyDown(GLFW_KEY_KP_4)?0xEF:0xFF;
	joyVal&=KeyDown(GLFW_KEY_KP_0)?0xDF:0xFF;
	VIA0_PinSetPIN_PA(joyVal);

	joyVal=0xFF;
	joyVal&=KeyDown(GLFW_KEY_KP_6)?0x7F:0xFF;
	VIA1_PinSetPIN_PB(joyVal);
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
	if (CheckKey(GLFW_KEY_PAGEUP))
	{
		playDown=1;
		recDown=playDown;
		ClearKey(GLFW_KEY_PAGEUP);
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
	if (CheckKey(GLFW_KEY_END))
	{
		playDown=0;
		recDown=0;
		ClearKey(GLFW_KEY_END);
		cntPos=0;
		casCount=0;
		casLevel=0;		
	}

	// Cassette deck
	if (playDown)
	{
		VIA0_PinSetPIN_PA(VIA0_PinGetPIN_PA()&0xBF);
	}
	if (playDown && recDown && (!VIA0_PinGetPIN_CA2())) // SAVING
	{
		uint8_t newRecLevel = (VIA1_PinGetPIN_PB()&0x08)>>3;
		casCount++;
		if (newRecLevel!=casLevel)
		{
			printf("Level Changed %08X\n",casCount);
			cntMax=cntPos;
			cntBuffer[cntPos++]=casCount;
			casCount=0;
			casLevel=newRecLevel;
		}
	}
	if (playDown && (!recDown) && (!VIA0_PinGetPIN_CA2())) // LOADING
	{
		casCount++;
		if (casCount>=cntBuffer[cntPos])
		{
			casLevel^=1;
			cntPos++;
			casCount=0;
		}
	}
	VIA1_PinSetPIN_CA1(casLevel);
}

uint8_t lastClk=12,lastDat=12;

void UpdateDiskInterface()
{
	// PA7 - ATN OUT -> PB7,CA1
	uint8_t tmp,newpa;
	uint8_t clk,dat,atn;

	clk=VIA1_PinGetPIN_CA2();
	dat=VIA1_PinGetPIN_CB2();
	atn=(VIA0_PinGetPIN_PA()&0x80)>>7;

	if (lastClk!=clk)
	{
		printf("Clk %s  atn %d\n",clk?"hi":"lo",atn);
		lastClk=clk;
	}
	if (lastDat!=dat)
	{
		printf("Dat %s  atn %d\n",dat?"hi":"lo",atn);
		lastDat=dat;
	}

	clk^=1;
	dat^=1;
	atn^=1;

	DISK_Tick(&clk,&atn,&dat);
	
	clk^=1;
	dat^=1;
	atn^=1;

	tmp=VIA0_PinGetPIN_PA()&0x7C;
	tmp|=clk;
	tmp|=dat<<1;
	tmp|=atn<<7;

	VIA0_PinSetPIN_PA(tmp);
#if 0
	tmp&=0x0A;
	newpa=VIA0_PinGetPIN_PA()&0xFC;
	newpa|=(tmp&0x08)>>3;
	newpa|=tmp&0x02;

	VIA0_PinSetPIN_PA(newpa);
#endif
#if 0
		if ((lastPb0&0x1A)!=(tmp&0x1A))
		{
			printf("Data From Disk Changed : %02X\n",tmp);
		}
		lastPb0=tmp;

		pb0&=0xFC;
		pb0|=(tmp&0x08)>>3;
		pb0|=(tmp&0x02);

		if (via[1].PCR&0x10)
		{
			if (via[1].CB1==0 && tmp&0x10)
			{
				printf("LH INT due to ATN ACK\n");
				doDebug=1;
				via[1].IFR=0x10;
			}
		}
		else
		{
			if (via[1].CB1==1 && (tmp&0x10)==0)
			{
				printf("HL INT due to ATN ACK\n");
				via[1].IFR=0x10;
			}
		}
		via[1].CB1=(tmp&0x10)>>4;
#endif
}

void UpdateHardware()
{
	UpdateKB();
	UpdateJoy();
	UpdateCassette();
	UpdateDiskInterface();
}

int main(int argc,char**argv)
{
	int w,h;
	double	atStart,now,remain;
	uint16_t bp;

	/// Initialize GLFW 
	glfwInit(); 

	// Open screen OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwOpenWindow( WIDTH, HEIGHT, GLFW_WINDOWED,"vic",NULL)) ) 
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
	if (DISK_InitialiseMemory())
		return -1;

	if (argc==2)
	{
		LoadTape(argv[1]);
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
	MAIN_PinSetPIN__IRQ(1);

	//dumpInstruction=100000;

	MAIN_PC=GetByte(0xFFFC);
	MAIN_PC|=GetByte(0xFFFD)<<8;

	bp = GetByte(0xc000);
	bp|=GetByte(0xC001)<<8;
	bp = GetByte(0xFFFE);
	bp|=GetByte(0xFFFF)<<8;

	bp=0xF7AF;

	printf("%04X\n",bp);

	printf("%02X != %02X\n",BRom[0xD487-0xC000],GetByte(0xD487));
	
	DISK_Reset();
	
#if 0
	uint8_t pb0=0xFF;
	via[0].CB1=1;
#endif

	VIA0_PinSetPIN__RES(0);
	VIA0_PinSetPIN_O2(0);
	VIA0_PinSetPIN_O2(1);
	VIA0_PinSetPIN_O2(0);
	VIA0_PinSetPIN_O2(1);
	VIA0_PinSetPIN_O2(0);
	VIA0_PinSetPIN__RES(1);

	VIA1_PinSetPIN__RES(0);
	VIA1_PinSetPIN_O2(0);
	VIA1_PinSetPIN_O2(1);
	VIA1_PinSetPIN_O2(0);
	VIA1_PinSetPIN_O2(1);
	VIA1_PinSetPIN_O2(0);
	VIA1_PinSetPIN__RES(1);

	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
	{
		static uint8_t lastPb0=0xff;
		uint16_t addr; 
		

		MAIN_PinSetPIN_O0(1);
		addr = MAIN_PinGetPIN_AB();

		if (MAIN_PinGetPIN_SYNC())
		{
//			if (MAIN_PinGetPIN_AB()==bp)
//				doDebug=1;

			if (doDebug)
			{
				Disassemble(addr,1);
				getch();
			}
		}

		VIA0_PinSetPIN_RW(MAIN_PinGetPIN_RW());
		VIA1_PinSetPIN_RW(MAIN_PinGetPIN_RW());

		VIA0_PinSetPIN_RS(addr&0xF);
		VIA1_PinSetPIN_RS(addr&0xF);

		if ((addr&0xFC00)==0x9000)
		{
			if (doDebug)
				printf("%04X : CS SETTING\n",addr);
			VIA0_PinSetPIN_CS((addr&0x0010)>>4);
			VIA1_PinSetPIN_CS((addr&0x0020)>>5);
		}
		else
		{
			if (doDebug)
				printf("%04X : CS CLEARING\n",addr);
			VIA0_PinSetPIN_CS(0x02|((addr&0x0010)>>4));
			VIA1_PinSetPIN_CS(0x02|((addr&0x0020)>>5));
		}

		VIA0_PinSetPIN_O2(1);
		VIA1_PinSetPIN_O2(1);


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


		VIA0_PinSetPIN_O2(0);
		VIA1_PinSetPIN_O2(0);
		
		UpdateHardware();

		MAIN_PinSetPIN__IRQ(VIA1_PinGetPIN__IRQ());

		lastBus=MAIN_PinGetPIN_DB();

		MAIN_PinSetPIN_O0(0);

		Tick6561();

		pixelClock++;

		if (pixelClock>=22152/* || stopTheClock*/)
		{
			static int normalSpeed=1;

			pixelClock-=22152;

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
				
			glfwPollEvents();
			
			kbuffer[0]=CheckKeys('1','3','5','7','9','-','=',GLFW_KEY_BACKSPACE);
			kbuffer[1]=CheckKeys('`','W','R','Y','I','P',']',GLFW_KEY_ENTER);
			kbuffer[2]=CheckKeys(GLFW_KEY_LCTRL,'A','D','G','J','L','\'',GLFW_KEY_RIGHT);
			kbuffer[3]=CheckKeys(GLFW_KEY_TAB,GLFW_KEY_LSHIFT,'X','V','N',',','/',GLFW_KEY_DOWN);
			kbuffer[4]=CheckKeys(GLFW_KEY_SPACE,'Z','C','B','M','.',GLFW_KEY_RSHIFT,GLFW_KEY_F1);
			kbuffer[5]=CheckKeys(GLFW_KEY_RCTRL,'S','F','H','K',';','#',GLFW_KEY_F3);
			kbuffer[6]=CheckKeys('Q','E','T','U','O','[','/',GLFW_KEY_F5);
			kbuffer[7]=CheckKeys('2','4','6','8','0','\\',GLFW_KEY_HOME,GLFW_KEY_F7);

			if (CheckKey(GLFW_KEY_INSERT))
			{
				ClearKey(GLFW_KEY_INSERT);
				normalSpeed^=1;
			}

			g_traceStep=0;
			
			now=glfwGetTime();

			remain = now-atStart;

			while ((remain<0.02f) && normalSpeed)
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

#if 0
///////////////////
//



uint8_t VIAGetByte(int chipNo,int regNo)
{
	if (doDebug)
		printf("R VIA%d %02X\n",chipNo+1,regNo);
	switch (regNo)
	{
		case 0:			//IRB
			via[chipNo].IFR&=~0x18;
			return (via[chipNo].IRB & (~via[chipNo].DDRB)) | (via[chipNo].ORB & via[chipNo].DDRB);
		case 1:			//IRA
			via[chipNo].IFR&=~0x03;
			//FALL through intended
		case 15:
			return (via[chipNo].IRA & (~via[chipNo].DDRA));
		case 2:
			return via[chipNo].DDRB;
		case 3:
			return via[chipNo].DDRA;
		case 4:
			via[chipNo].IFR&=~0x40;
			return (via[chipNo].T1C&0xFF);
		case 5:
			return (via[chipNo].T1C>>8);
		case 6:
			return via[chipNo].T1LL;
		case 7:
			return via[chipNo].T1LH;
		case 8:
			via[chipNo].IFR&=~0x20;
			return (via[chipNo].T2C&0xFF);
		case 9:
			return (via[chipNo].T2C>>8);
		case 10:
			via[chipNo].IFR&=~0x04;
			return via[chipNo].SR;
		case 11:
			return via[chipNo].ACR;
		case 12:
			return via[chipNo].PCR;
		case 13:
			return via[chipNo].IFR;
		case 14:
			return via[chipNo].IER & 0x7F;
	}
}

void VIASetByte(int chipNo,int regNo,uint8_t byte)
{
//	if (doDebug)
//		printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
	switch (regNo)
	{
		case 0:
			via[chipNo].IFR&=~0x18;
//			if (via[chipNo].ORB!=(byte&via[chipNo].DDRB))
//				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			via[chipNo].ORB=byte&via[chipNo].DDRB;
			break;
		case 1:
			via[chipNo].IFR&=~0x03;
			//FALL through intended
		case 15:
//			if (via[chipNo].ORA!=(byte&via[chipNo].DDRA))
			if (chipNo==0)
			{
				printf("CA2/CB2 %02X/%02X\n",via[1].CA2,via[1].CB2);
				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			}
			via[chipNo].ORA=byte&via[chipNo].DDRA;
			break;
		case 2:
			if (via[chipNo].DDRB!=byte)
				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			via[chipNo].DDRB=byte;
			break;
		case 3:
			if (via[chipNo].DDRA!=byte)
				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			via[chipNo].DDRA=byte;
			break;
		case 4:
			via[chipNo].T1LL=byte;
			break;
		case 5:
			via[chipNo].T1LH=byte;
			via[chipNo].T1C=byte<<8;
			via[chipNo].T1C|=via[chipNo].T1LL;
			via[chipNo].IFR&=~0x40;
			break;
		case 6:
			via[chipNo].T1LL=byte;
			break;
		case 7:
			via[chipNo].T1LH=byte;
			via[chipNo].IFR&=~0x40;
			break;
		case 8:
			via[chipNo].T2LL=byte;
			break;
		case 9:
			via[chipNo].T2TimerOff=0;
			via[chipNo].T2C=byte<<8;
			via[chipNo].T2C|=via[chipNo].T2LL;
			via[chipNo].IFR&=~0x20;
			break;
		case 10:
			via[chipNo].IFR&=~0x04;
			via[chipNo].SR=byte;
			break;
		case 11:
			if (via[chipNo].ACR!=byte)
				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			via[chipNo].ACR=byte;
			break;
		case 12:
			if (via[chipNo].PCR!=byte)
				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			via[chipNo].PCR=byte;
			break;
		case 13:
			byte&=0x7F;
			byte=~byte;
			via[chipNo].IFR&=byte;
			break;
		case 14:
			if (byte&0x80)
			{
				via[chipNo].IER|=byte&0x7F;
			}
			else
			{
				via[chipNo].IER&=~(byte&0x7F);
			}
			printf("%d : byte %02X, IER %02X\n",chipNo+1,byte,via[chipNo].IER);
			break;
	}
}

void VIATick(int chipNo,uint8_t PB0)
{
	via[chipNo].IRA=0x00;
	if (chipNo==0)
	{
		via[chipNo].IRB=PB0;
	}
	else
	{
		via[chipNo].IRB=0x00;
	}

	if (via[chipNo].T1C)
	{
		via[chipNo].T1C--;
		if (via[chipNo].T1C==0)
		{
			via[chipNo].IFR|=0x40;
			if (via[chipNo].ACR&0x40)
			{
				via[chipNo].T1C=via[chipNo].T1LH<<8;
				via[chipNo].T1C|=via[chipNo].T1LL;
			}
//			printf("T1 Counter Underflow On VIA %d\n",chipNo);
		}
	}
	via[chipNo].T2C--;
	if ((via[chipNo].T2C==0) && (via[chipNo].T2TimerOff==0))
	{
		via[chipNo].IFR|=0x20;
		via[chipNo].T2TimerOff=1;
//		printf("T2 Counter Underflow On VIA %d\n",chipNo);
	}

	switch (via[chipNo].PCR&0x0E)
	{
		case 0x0:
			//Input Mode - 
			via[chipNo].CA2=0;
			break;
		case 0x2:
		case 0x4:
		case 0x6:
		case 0x8:
		case 0xA:
			printf("Warning Unsupported PCR CA2 mode (%d)%d\n",chipNo+1,(via[chipNo].PCR&0xE)>>1);
			break;
		case 0xC:
			via[chipNo].CA2=0x0;
			break;
		case 0xE:
			via[chipNo].CA2=0x1;
			break;
	}
	switch (via[chipNo].PCR&0xE0)
	{
		case 0x00:
			//Input Mode -
			break;
		case 0x20:
		case 0x40:
		case 0x60:
		case 0x80:
		case 0xA0:
			printf("Warning Unsupported PCR CB2 mode (%d)%d\n",chipNo+1,(via[chipNo].PCR&0xE)>>1);
			break;
		case 0xC0:
			via[chipNo].CB2=0x0;
			break;
		case 0xE0:
			via[chipNo].CB2=0x1;
			break;
	}

}
#endif

////////////6561////////////////////

uint32_t RASTER_CNT=0;

uint8_t GetByte6561(int regNo)
{
//	printf("R 6561 %02X\n",regNo);
	switch(regNo)
	{
		case 0:
			return CTRL_1;
		case 1:
			return CTRL_2;
		case 2:
			return CTRL_3;
		case 3:
			return CTRL_4;
		case 4:
			return CTRL_5;
		case 5:
			return CTRL_6;
		case 6:
			return CTRL_7;
		case 7:
			return CTRL_8;
		case 8:
			return CTRL_9;
		case 9:
			return CTRL_10;
		case 10:
			return CTRL_11;
		case 11:
			return CTRL_12;
		case 12:
			return CTRL_13;
		case 13:
			return CTRL_14;
		case 14:
			return CTRL_15;
		case 15:
			return CTRL_16;
	}
}
uint16_t	channel1Cnt=0;
uint16_t	channel2Cnt=0;
uint16_t	channel3Cnt=0;
uint16_t	channel4Cnt=0;
uint16_t	noiseShift=1;

void SetByte6561(int regNo,uint8_t byte)
{
//	printf("W 6561 %02X,%02X\n",regNo,byte);
	switch(regNo)
	{
		case 0:
			CTRL_1=byte;
			break;
		case 1:
			CTRL_2=byte;
			break;
		case 2:
			CTRL_3=byte;
			break;
		case 3:
			CTRL_4&=0x80;
			CTRL_4|=byte&0x7F;
			break;
		case 4:
			break;
		case 5:
			CTRL_6=byte;
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			break;
		case 9:
			break;
		case 10:
			CTRL_11=byte;
			channel1Cnt=(255-byte)<<8;
			break;
		case 11:
			CTRL_12=byte;
			channel2Cnt=(255-byte)<<7;
			break;
		case 12:
			CTRL_13=byte;
			channel3Cnt=(255-byte)<<6;
			break;
		case 13:
			CTRL_14=byte;
			channel4Cnt=(255-byte)<<5;
			break;
		case 14:
			CTRL_15=byte;
			break;
		case 15:
			CTRL_16=byte;
			break;
	}
}

uint8_t xCnt=0;

int16_t	channel1Level=0;
int16_t channel2Level=0;
int16_t	channel3Level=0;
int16_t channel4Level=0;

uint8_t GetByteFrom6561(uint16_t addr)
{
	// A0-A12 are connected as expected in vic20... a13 is inverted and connected to a15
	addr=(addr&0x1FFF)|((addr&0x2000)<<2);
	addr^=0x8000;
	return GetByte(addr);
}

void Tick6561()
{
	uint32_t* outputTexture = (uint32_t*)videoMemory[MAIN_WINDOW];
	uint32_t originX;
	uint32_t originY;
	uint32_t length;
	uint32_t height;
	uint32_t borderCol;

	borderCol=CTRL_16&0x07;
	borderCol=cTable[borderCol];

	// 4 pixels per clock		71 clocks per line (PAL)		312 lines per frame		vblank 0-27

	if (RASTER_CNT>=28 && xCnt>=5 && xCnt<=71-5)		// !Blanking area
	{
		uint8_t doubleHeight=0;

		originX=CTRL_1&0x7F;		// *4 pixels
		length=CTRL_3&0x7F;		// num 8 pixel columns
		length<<=1;
		originY=CTRL_2;
		originY<<=1;
		height=CTRL_4&0x7E;
		if (CTRL_4&0x01)
		{
			doubleHeight=1;
		}

		height<<=2+doubleHeight;

		if ((xCnt<originX) || (xCnt>=(originX+length)) || (RASTER_CNT<originY) || (RASTER_CNT>=(originY+height)))
		{
			// Border
			outputTexture[(xCnt-5)*4 + 0 + ((RASTER_CNT-28)*WIDTH)]=borderCol;
			outputTexture[(xCnt-5)*4 + 1 + ((RASTER_CNT-28)*WIDTH)]=borderCol;
			outputTexture[(xCnt-5)*4 + 2 + ((RASTER_CNT-28)*WIDTH)]=borderCol;
			outputTexture[(xCnt-5)*4 + 3 + ((RASTER_CNT-28)*WIDTH)]=borderCol;
		}
		else
		{
			int xx;
			uint16_t addr,addr1,addr2;
			uint16_t chaddr;
			uint16_t caddr;
			uint32_t paper;
			uint32_t auxcol;

			addr1=CTRL_6&0xF0;
			addr2=CTRL_3&0x80;
			addr=(addr1<<6)|(addr2<<2);

			chaddr=CTRL_6&0x0F;
			chaddr<<=10;

			caddr=CTRL_3&0x80;
			caddr<<=2;
			caddr|=0x1400;

			paper=(CTRL_16&0xF0)>>4;
			paper=cTable[paper];

			auxcol=(CTRL_15&0xF0)>>4;
			auxcol=cTable[auxcol];

			uint32_t x = xCnt-originX;
			uint32_t y = RASTER_CNT-originY;

			uint16_t index;
			uint32_t screenAddress = x/2;
			if (doubleHeight)
				screenAddress+= (y/16)*(length/2);
			else
				screenAddress+= (y/8)*(length/2);

			uint32_t rcol= GetByteFrom6561(caddr+screenAddress);
			uint32_t col;
			col=cTable[rcol&7];
			index=GetByteFrom6561(addr+screenAddress);
			index<<=3+doubleHeight;
			if (doubleHeight)
				index+=y&15;
			else
				index+=y&7;

			uint8_t byte = GetByteFrom6561((chaddr+index));
			
			if (rcol&0x08)
			{
				uint8_t sMask = 0xC0;
				if (x&1)
					sMask=0x0C;
				for (xx=0;xx<2;xx++)
				{
					switch (byte&sMask)
					{
						case 0x00:
							outputTexture[(xCnt-5)*4 + (xx*2)+0 + ((RASTER_CNT-28)*WIDTH)]=paper;
							outputTexture[(xCnt-5)*4 + (xx*2)+1 + ((RASTER_CNT-28)*WIDTH)]=paper;
							break;
						case 0x40:
						case 0x10:
						case 0x04:
						case 0x01:
							outputTexture[(xCnt-5)*4 + (xx*2)+0 + ((RASTER_CNT-28)*WIDTH)]=borderCol;
							outputTexture[(xCnt-5)*4 + (xx*2)+1 + ((RASTER_CNT-28)*WIDTH)]=borderCol;
							break;
						case 0x80:
						case 0x20:
						case 0x08:
						case 0x02:
							outputTexture[(xCnt-5)*4 + (xx*2)+0 + ((RASTER_CNT-28)*WIDTH)]=col;
							outputTexture[(xCnt-5)*4 + (xx*2)+1 + ((RASTER_CNT-28)*WIDTH)]=col;
							break;
						default:
							outputTexture[(xCnt-5)*4 + (xx*2)+0 + ((RASTER_CNT-28)*WIDTH)]=auxcol;
							outputTexture[(xCnt-5)*4 + (xx*2)+1 + ((RASTER_CNT-28)*WIDTH)]=auxcol;
							break;
					}
					sMask>>=2;
				}
			}
			else
			{
				uint8_t sMask = 0x80;
				if (CTRL_16&0x08)
					byte^=0xFF;
				if (x&1)
					sMask=0x08;
				for (xx=0;xx<4;xx++)
				{
					if (byte&sMask)
					{
						outputTexture[(xCnt-5)*4 + xx + ((RASTER_CNT-28)*WIDTH)]=paper;
					}
					else
					{
						outputTexture[(xCnt-5)*4 + xx + ((RASTER_CNT-28)*WIDTH)]=col;
					}
					sMask>>=1;
				}
			}
		}
	}
	xCnt++;
	if (xCnt>=71)
	{
		xCnt=0;
		RASTER_CNT++;
		if (RASTER_CNT>=312)
			RASTER_CNT=0;
		CTRL_5=(RASTER_CNT>>1)&0xFF;
		CTRL_4&=0x7F;
		CTRL_4|=(RASTER_CNT&0x01)<<7;
	}

	if (CTRL_11&0x80)
	{
		channel1Cnt--;
		if (channel1Cnt==0xFFFF)
		{
			channel1Cnt=(255-CTRL_11)<<8;
			channel1Level^=1;
		}
	}
	else
	{
		channel1Cnt=0;
	}
	if (CTRL_12&0x80)
	{
		channel2Cnt--;
		if (channel2Cnt==0xFFFF)
		{
			channel2Cnt=(255-CTRL_12)<<7;
			channel2Level^=1;
		}
	}
	else
	{
		channel2Cnt=0;
	}
	if (CTRL_13&0x80)
	{
		channel3Cnt--;
		if (channel3Cnt==0xFFFF)//(channel3Cnt/*>>6*/)>=CTRL_13&0x7F)
		{
			channel3Cnt=(255-CTRL_13)<<6;
			channel3Level^=1;
		}
	}
	else
	{
		channel3Cnt=0;
	}
	if (CTRL_14&0x80)
	{
		channel4Cnt--;
		if (channel4Cnt==0xFFFF)//(channel3Cnt/*>>6*/)>=CTRL_13&0x7F)
		{
			uint16_t newBit;
			channel4Cnt=(255-CTRL_14)<<5;
			channel4Level^=noiseShift&1;
			newBit=((noiseShift&0x80)>>7) ^ (noiseShift&1);
			noiseShift>>=1;
			noiseShift|=newBit<<15;
		}
	}
	else
	{
		channel4Cnt=0;
	}

	_AudioAddData(0,channel1Level*256*(CTRL_15&0x0F));
	_AudioAddData(1,channel2Level*256*(CTRL_15&0x0F));
	_AudioAddData(2,channel3Level*256*(CTRL_15&0x0F));
	_AudioAddData(3,channel4Level*256*(CTRL_15&0x0F));

	UpdateAudio();

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
			res+=casLevel?1024:-1024;

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
		free(tapeBuffer);
		return 0;
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
			printf("Pulse Length : %d\n",cntBuffer[cntPos]);
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

void LoadTape(const char* fileName)
{
	if (LoadTAP(fileName))
		return;
	if (LoadPRG(fileName))
		return;
	cntPos=0;
	cntMax=0;
}

