/*
 * ZXSpectrum
 *
 *  Uses Z80 step core built in EDL
 *  Rest is C for now
 */

#include <GL/glfw3.h>
#include <GL/glext.h>

#include <al.h>
#include <alc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

uint16_t PinGetPIN_A();
uint8_t PinGetPIN_D();
void PinSetPIN_D(uint8_t);
void PinSetPIN__CLK(uint8_t);
uint8_t PinGetPIN__M1();
uint8_t PinGetPIN__MREQ();
uint8_t PinGetPIN__WR();
uint8_t PinGetPIN__RD();
uint8_t PinGetPIN__RFSH();
uint8_t PinGetPIN__IORQ();
void PinSetPIN__WAIT(uint8_t);
void PinSetPIN__INT(uint8_t);
void PinSetPIN__NMI(uint8_t);
void PinSetPIN__RESET(uint8_t);
void PinSetPIN__BUSRQ(uint8_t);

unsigned char Rom[0x4000];
unsigned char Ram[0xC000];

void AudioKill();
void AudioInitialise();
void UpdateAudio();
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
/*	if (LoadRom(0x0000,0x1000,"roms/test.rom"))
		return 1;
*/
	return 0;
}

int masterClock=0;
int pixelClock=0;
int tapeClock=0;

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

extern unsigned short PC;
extern unsigned char CYCLES;

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

int tapePaused=1;
int syncOff=0;

void LoadTape();
void LoadSNA();
void LoadTAP();
void LoadTZX();
void UpdateTZX(uint8_t cycles);
uint8_t GetTZXLevel();
void UpdateTAP(uint8_t cycles);
uint8_t GetTAPLevel();
void UpdateTape(uint8_t cycles);
uint8_t GetTapeLevel();

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
/*		if (GetTZXLevel()>=0x80)
		{
			retVal|=0x40;
		}*/
		if (GetTAPLevel()>=0x80)
		{
			retVal|=0x40;
		}
/*		if (GetTapeLevel()>=0x80)
		{
			retVal|=0x40;
		}*/

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
#if 0
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
#endif
	return numBytes+1;
}

void SHOW_PINS()
{
	printf("%s %s %s %s %s %s %04X %02X\n",
			PinGetPIN__M1()?"  ":"M1",
			PinGetPIN__MREQ()?"    ":"MREQ",
			PinGetPIN__RD()?"  ":"RD",
			PinGetPIN__WR()?"  ":"WR",
			PinGetPIN__RFSH()?"    ":"RFSH",
			PinGetPIN__IORQ()?"    ":"IORQ",
			PinGetPIN_A(),
			PinGetPIN_D());
}

#define SCREEN_ON	1

void DisassembleRange(unsigned int start,unsigned int end)
{
	while (start<end)
	{
		start+=Disassemble(start,0);
	}
}	
	
int main(int argc,char**argv)
{
	double	atStart,now,remain;
	int a;
	int intClocks=0;
	int curRow=0;

#if SCREEN_ON
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
#endif
	//////////////////

	LoadTAP();
	LoadTape();
	LoadTZX();
	if (InitialiseMemory())
		return -1;
	
	PinSetPIN__WAIT(1);
	PinSetPIN__INT(1);
	PinSetPIN__NMI(1);
	PinSetPIN__RESET(1);
	PinSetPIN__BUSRQ(1);

	PinSetPIN__RESET(0);
	PinSetPIN__CLK(1);
	PinSetPIN__CLK(0);
	PinSetPIN__CLK(1);
	PinSetPIN__CLK(0);
	PinSetPIN__CLK(1);
	PinSetPIN__CLK(0);
	PinSetPIN__RESET(1);
	
//	DisassembleRange(0x0000,0x4000);

	LoadSNA();

	while (1==1)
	{
		static int doDebug=1;
		PinSetPIN__INT(1);
		if (intClocks!=0)
		{
			PinSetPIN__INT(0);
			intClocks--;
		}

		PinSetPIN__CLK(1);
#if 0
		if ((!PinGetPIN__M1()) && PinGetPIN__MREQ())		// NOT PERFECT, SINCE IT WILL CATCH INTERRUPT REQUESTS.. 
		{
#if 0
			if (PinGetPIN_A()==0x056C)
				doDebug=1;
#endif
			// First cycle of a new instruction
			if (doDebug)
			{
				Disassemble(PinGetPIN_A(),1);
			}
		}
#endif
		if ((!PinGetPIN__M1()) && PinGetPIN__IORQ())
		{
			PinSetPIN_D(0xFF);
		}
		if ((!PinGetPIN__MREQ()) && (!PinGetPIN__RD()))
		{
			uint16_t addr = PinGetPIN_A();
			uint8_t  data = GetByte(addr);
			PinSetPIN_D(data);
		}
		if ((!PinGetPIN__IORQ()) && (!PinGetPIN__RD()))
		{
			uint16_t addr = PinGetPIN_A();
			uint8_t  data = GetPort(addr);
			PinSetPIN_D(data);
		}
		if ((!PinGetPIN__MREQ()) && (!PinGetPIN__WR()))
		{
			SetByte(PinGetPIN_A(),PinGetPIN_D());
		}
		if ((!PinGetPIN__IORQ()) && (!PinGetPIN__WR()))
		{
			SetPort(PinGetPIN_A(),PinGetPIN_D());
		}
	
		PinSetPIN__CLK(0);

		masterClock+=1;

		UpdateTape(1);
		UpdateTAP(1);
		UpdateTZX(1);
		UpdateAudio();
		
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
#if 0
			if (glfwGetKey(windows[MAIN_WINDOW],' '))
			{
				doDebug=1;
			}
#endif

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
uint32_t			tapePos=0;
uint32_t			tapeCycles=0;
uint8_t				casLevel=0;
uint8_t*			RawTapFile=NULL;
uint32_t			RawTapLength=0;

unsigned char *qLoad(char *romName,uint32_t *length)
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

uint8_t tapPhase;		//0 - initial (start of new block)
uint8_t	tapLevel;
uint32_t tapPosition;
uint32_t tapLength;
uint8_t *tap=NULL;

void LoadTAP()
{
	uint32_t a;
	uint16_t blockLength;
	uint32_t curPos=0;

	tap=qLoad(".tap",&tapLength);
	if (!tap)
		return;

	// Test.. Dump block infos
	
	while (curPos<tapLength)
	{
		// First 2 bytes is length of next item
		blockLength=tap[curPos++];
		blockLength|=tap[curPos++]<<8;
		if (tap[curPos]<0x80)
		{
			// Header block
			curPos++;
			switch (tap[curPos++])
			{
				case 0:
					printf("BasicProgram: \n");
					break;
				case 1:
					printf("NumberArray: \n");
					break;
				case 2:
					printf("CharacterArray: \n");
					break;
				case 3:
					printf("Code: \n");
					break;
				default:
					printf("Unknown: \n");
					break;
			}
			for (a=0;a<10;a++)
			{
				printf("%c",tap[curPos++]);
			}
			printf("\nData Length : %02X%02X\n",tap[curPos+1],tap[curPos]);
			curPos+=2;
			printf("Param1 : %02X%02X\n",tap[curPos+1],tap[curPos]);
			curPos+=2;
			printf("Param2 : %02X%02X\n",tap[curPos+1],tap[curPos]);
			curPos+=2;
			printf("CheckByte: %02X\n",tap[curPos++]);
		}
		else
		{
			printf("DataBlock: Length %04X\n",blockLength-2);
			// Data block
			for (a=0;a<blockLength;a++)
			{
				curPos++;
			}
		}
	}

	tapPosition=0;
	tapPhase=0;
	tapLevel=0;
}

uint16_t tapDataLength;
uint8_t tapDataBit;
uint16_t tapPilotLength;
uint32_t tapPilotPulseLength;
uint32_t tapTStateCounter;

void UpdateTAP(uint8_t cycles)
{
	if (!tap || tapePaused)
		return;

	switch (tapPhase)
	{
		case 0:								// New block
			if (tapPosition>=tapLength)
				tapPosition=0;

			tapDataLength=tap[tapPosition++];
			tapDataLength|=tap[tapPosition++]<<8;
			if (tap[tapPosition]<0x80)
			{
				// Header Pilot Pulse
				tapPilotLength=8063;
				tapPilotPulseLength=2168;
			}
			else
			{
				// data Pilot Pulse
				tapPilotLength=3223;
				tapPilotPulseLength=2168;
			}
			tapTStateCounter=0;
			tapPhase=1;
			// Fall through?
		case 1:								// Pilot pulse
			tapTStateCounter++;
			if (tapTStateCounter>=tapPilotPulseLength)
			{
				tapTStateCounter-=tapPilotPulseLength;
				tapLevel=~tapLevel;
				tapPilotLength--;
				if (tapPilotLength==0)
				{
					tapPhase=2;
					tapPilotPulseLength=667;
				}
			}
			break;
		case 2:								// Sync 1 pulse
			tapTStateCounter++;
			if (tapTStateCounter>=tapPilotPulseLength)
			{
				tapTStateCounter-=tapPilotPulseLength;
				tapLevel=~tapLevel;
				tapPhase=3;
				tapPilotPulseLength=735;
			}
			break;
		case 3:								// Sync 2 pulse
			tapTStateCounter++;
			if (tapTStateCounter>=tapPilotPulseLength)
			{
				tapLevel=~tapLevel;
				tapTStateCounter-=tapPilotPulseLength;
				tapDataBit=0x80;
				tapPhase=4;
			}
			break;
		case 4:								// Data
			// For each byte in tapDataLength
			// We get a bit, set the correct pulse length
			// rinse and repeat
			
			if (tap[tapPosition]&tapDataBit)
			{
				tapPilotPulseLength=1710;
			}
			else
			{
				tapPilotPulseLength=855;
			}
			tapPhase=5;
			// Fallthrough
		case 5:
			tapTStateCounter++;
			if (tapTStateCounter>=tapPilotPulseLength)
			{
				tapPhase=6;
				tapTStateCounter-=tapPilotPulseLength;
				tapLevel=~tapLevel;
			}
			break;
		case 6:
			tapTStateCounter++;
			if (tapTStateCounter>=tapPilotPulseLength)
			{
				tapPhase=4;
				tapTStateCounter-=tapPilotPulseLength;
				tapLevel=~tapLevel;
				tapDataBit>>=1;
				if (tapDataBit==0)
				{
					tapDataBit=0x80;
					tapPosition++;
					tapDataLength--;
					if (tapDataLength==0)
					{
						tapPilotPulseLength=50*ROWS_PER_VBLANK*TSTATES_PER_ROW;
						tapPhase=7;
					}
					else
					{
					}

				}
			}
			break;
		case 7:								// Silence
			tapTStateCounter++;
			if (tapTStateCounter>=tapPilotPulseLength)
			{
				tapLevel=~tapLevel;
				tapPhase=0;
			}
			break;
	}
}

uint8_t GetTAPLevel()
{
	return tapLevel;
}

uint8_t tzxPhase;		//0 - initial (start of new block)
uint8_t	tzxLevel;
uint32_t tzxPosition;
uint32_t tzxLength;
uint32_t tzxTStateCounter;
uint8_t *tzx=NULL;

void LoadTZX()
{
	uint32_t a;
	uint32_t blockLength;
	uint32_t curPos=0;

	tzx=qLoad(".tzx",&tzxLength);
	if (!tzx)
		return;

	// Check header
	if (strncmp(tzx,"ZXTape!",7)!=0)
		return;
	if (tzx[7]!=0x1A)
		return;

	printf("TZX: Revision : %d.%d\n",tzx[8],tzx[9]);

	// Test.. Dump block infos
	curPos=10;
	while (curPos<tzxLength)
	{
		uint8_t blockId=tzx[curPos++];
		switch (blockId)
		{
			case 0x10:
				printf("ID10 : Standard Speed Block\n");
				printf("Delay After Block : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				blockLength=tzx[curPos++];
				blockLength|=tzx[curPos++]<<8;
				printf("Data Length : %08X\n",blockLength);
				curPos+=blockLength;
				break;
			case 0x11:
				printf("ID11 : Turbo Speed Block\n");
				printf("Pilot Length : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("Sync1 Length : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("Sync2 Length : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("Zero Length : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("One Length : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("Num Pilot Pulses : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("Used bits in last byte : %02X\n",tzx[curPos++]);
				printf("Delay After Block : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				blockLength=tzx[curPos++];
				blockLength|=tzx[curPos++]<<8;
				blockLength|=tzx[curPos++]<<16;
				printf("Data Length : %08X\n",blockLength);
				curPos+=blockLength;
				break;
			case 0x12:
				printf("ID12 : Pure Tone\n");
				printf("Pulse Length : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("Number Of Pulses : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				break;
			case 0x13:
				printf("ID13 : Pulse Sequence\n");
				printf("Number Pulses : %02X\n",tzx[curPos]);
				curPos+=2*tzx[curPos];
				break;
			case 0x14:
				printf("ID14 : Pure Data Block\n");
				printf("Zero Length : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("One Length : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				printf("Used bits in last byte : %02X\n",tzx[curPos++]);
				printf("Delay After Block : %02X%02X\n",tzx[curPos+1],tzx[curPos]);
				curPos+=2;
				blockLength=tzx[curPos++];
				blockLength|=tzx[curPos++]<<8;
				blockLength|=tzx[curPos++]<<16;
				printf("Data Length : %08X\n",blockLength);
				curPos+=blockLength;
				break;
			case 0x21:
				printf("ID21 : Group Start\n");
				printf("Data Length : %02X\n",tzx[curPos]);
				curPos+=tzx[curPos]+1;
				break;
			case 0x22:
				printf("ID22 : Group End\n");
				break;
			case 0x30:
				printf("ID30 : Text Description\n");
				printf("Data Length : %02X\n",tzx[curPos]);
				curPos+=tzx[curPos]+1;
				break;
			case 0x32:
				printf("ID32 : Archive Info\n");
				blockLength=tzx[curPos++];
				blockLength|=tzx[curPos++]<<8;
				printf("Data Length : %08X\n",blockLength);
				curPos+=blockLength;
				break;
			default:
				printf("ID%02X\n",blockId);
				exit(-1);
				break;

		}
	}

	tzxPosition=10;
	tzxPhase=0;
	tzxLevel=0;
	tzxTStateCounter=0;
}

uint32_t tzxDataLength;
uint8_t tzxDataBit;
uint16_t tzxPilotLength;
uint32_t tzxPilotPulseLength;

uint16_t tzxParamPilotLength;
uint32_t tzxParamPilotPulseLength;
uint32_t tzxParamSync1Length;
uint32_t tzxParamSync2Length;
uint32_t tzxParamOneLength;
uint32_t tzxParamZeroLength;
uint32_t tzxParamDelayLength;

uint8_t GetNextBlock()
{
	while (1)
	{
		uint32_t blockLength;
		
		if (tzxPosition>=tzxLength)
		{
				tzxPosition=10;
		}

		{
			uint8_t blockId=tzx[tzxPosition++];
			switch (blockId)
			{
				case 0x10:
					printf("Standard\n");
					tzxParamDelayLength=tzx[tzxPosition++];
					tzxParamDelayLength|=tzx[tzxPosition++]<<8;
					tzxParamDelayLength=tzxParamDelayLength*3494.4f;
					tzxDataLength=tzx[tzxPosition++];
					tzxDataLength|=tzx[tzxPosition++]<<8;
					tzxParamPilotPulseLength=2168;
					if (tzx[tzxPosition]<0x80)
					{
						// Header Pilot Pulse
						tzxParamPilotLength=8063;
					}
					else
					{
						// data Pilot Pulse
						tzxParamPilotLength=3223;
					}
					tzxParamSync1Length=667;
					tzxParamSync2Length=735;
					tzxParamOneLength=1710;
					tzxParamZeroLength=855;
					return 1;
				case 0x11:
					printf("Speed\n");
					tzxParamPilotPulseLength=tzx[tzxPosition++];
					tzxParamPilotPulseLength|=tzx[tzxPosition++]<<8;
					tzxParamSync1Length=tzx[tzxPosition++];
					tzxParamSync1Length|=tzx[tzxPosition++]<<8;
					tzxParamSync2Length=tzx[tzxPosition++];
					tzxParamSync2Length|=tzx[tzxPosition++]<<8;
					tzxParamZeroLength=tzx[tzxPosition++];
					tzxParamZeroLength|=tzx[tzxPosition++]<<8;
					tzxParamOneLength=tzx[tzxPosition++];
					tzxParamOneLength|=tzx[tzxPosition++]<<8;
					tzxParamPilotLength=tzx[tzxPosition++];
					tzxParamPilotLength|=tzx[tzxPosition++]<<8;
					tzxPosition++;	// Used bits not handled yet
					tzxParamDelayLength=tzx[tzxPosition++];
					tzxParamDelayLength|=tzx[tzxPosition++]<<8;
					tzxParamDelayLength=tzxParamDelayLength*3494.4f;
					tzxDataLength=tzx[tzxPosition++];
					tzxDataLength|=tzx[tzxPosition++]<<8;
					tzxDataLength|=tzx[tzxPosition++]<<16;
					return 1;
				case 0x14:
					printf("Pure Data\n");
					tzxParamZeroLength=tzx[tzxPosition++];
					tzxParamZeroLength|=tzx[tzxPosition++]<<8;
					tzxParamOneLength=tzx[tzxPosition++];
					tzxParamOneLength|=tzx[tzxPosition++]<<8;
					tzxPosition++;	// Used bits not handled yet
					tzxParamDelayLength=tzx[tzxPosition++];
					tzxParamDelayLength|=tzx[tzxPosition++]<<8;
					tzxParamDelayLength=tzxParamDelayLength*3494.4f;
					tzxDataLength=tzx[tzxPosition++];
					tzxDataLength|=tzx[tzxPosition++]<<8;
					tzxDataLength|=tzx[tzxPosition++]<<16;
					return 4;
				case 0x21:
					printf("Group Start\n");
					tzxPosition+=tzx[tzxPosition]+1;
					break;
				case 0x22:
					printf("Group End\n");
					break;
				case 0x30:
					printf("Text Description\n");
					tzxPosition+=tzx[tzxPosition]+1;
					break;
				case 0x32:
					printf("Archive Info\n");
					blockLength=tzx[tzxPosition++];
					blockLength|=tzx[tzxPosition++]<<8;
					tzxPosition+=blockLength;
					break;
				default:
					printf("OH DEAR\n");
					break;
			}
		}
	}
}

void UpdateTZX(uint8_t cycles)
{
	int redo;
	if (!tzx || tapePaused)
		return;
crap:
	redo=0;
	switch (tzxPhase)
	{
		case 0:								// New block
			if (tzxPosition>=tzxLength)
				tzxPosition=10;

			tzxPhase = GetNextBlock();		// Will only work for ID10 and ID11 blocks at present
				
			tzxPilotLength=tzxParamPilotLength;
			tzxPilotPulseLength=tzxParamPilotPulseLength;
			if (tzxPhase!=1)
			{
				redo=1;
				break;
			}
			// Fall through?
		case 1:								// Pilot pulse
			tzxTStateCounter++;
			if (tzxTStateCounter>=tzxPilotPulseLength)
			{
				tzxTStateCounter-=tzxPilotPulseLength;
				tzxLevel=~tzxLevel;
				tzxPilotLength--;
				if (tzxPilotLength==0)
				{
					tzxPhase=2;
					tzxPilotPulseLength=tzxParamSync1Length;
				}
			}
			break;
		case 2:								// Sync 1 pulse
			tzxTStateCounter++;
			if (tzxTStateCounter>=tzxPilotPulseLength)
			{
				tzxTStateCounter-=tzxPilotPulseLength;
				tzxLevel=~tzxLevel;
				tzxPhase=3;
				tzxPilotPulseLength=tzxParamSync2Length;
			}
			break;
		case 3:								// Sync 2 pulse
			tzxTStateCounter++;
			if (tzxTStateCounter>=tzxPilotPulseLength)
			{
				tzxLevel=~tzxLevel;
				tzxTStateCounter-=tzxPilotPulseLength;
				tzxDataBit=0x80;
				tzxPhase=4;
			}
			break;
		case 4:								// Data
			// For each byte in tapDataLength
			// We get a bit, set the correct pulse length
			// rinse and repeat
			
			if (tzx[tzxPosition]&tzxDataBit)
			{
				tzxPilotPulseLength=tzxParamOneLength;
			}
			else
			{
				tzxPilotPulseLength=tzxParamZeroLength;
			}
			tzxPhase=5;
			// Fallthrough
		case 5:
			tzxTStateCounter++;
			if (tzxTStateCounter>=tzxPilotPulseLength)
			{
				tzxPhase=6;
				tzxTStateCounter-=tzxPilotPulseLength;
				tzxLevel=~tzxLevel;
			}
			break;
		case 6:
			tzxTStateCounter++;
			if (tzxTStateCounter>=tzxPilotPulseLength)
			{
				tzxPhase=4;
				tzxTStateCounter-=tzxPilotPulseLength;
				tzxLevel=~tzxLevel;
				tzxDataBit>>=1;
				if (tzxDataBit==0)
				{
					tzxDataBit=0x80;
					tzxPosition++;
					tzxDataLength--;
					if (tzxDataLength==0)
					{
						tzxPilotPulseLength=tzxParamDelayLength;
						if (tzxPilotPulseLength==0)
							tzxPhase=0;
						else
							tzxPhase=7;
					}
					else
					{
					}

				}
			}
			break;
		case 7:								// Silence
			tzxTStateCounter++;
			if (tzxTStateCounter>=tzxPilotPulseLength)
			{
				tzxTStateCounter-=tzxPilotPulseLength;
				tzxLevel=0;//~tzxLevel;
				tzxPhase=0;
			}
			break;
	}
	if (redo)
		goto crap;
}

uint8_t GetTZXLevel()
{
	return tzxLevel;
}

void LoadTape()
{
	RawTapFile=qLoad(".raw",&RawTapLength);
	if (!RawTapFile)
	{
		printf("Failed to load file\n");
		RawTapFile=malloc(1);
		RawTapLength=1;
	}
	tapePos=0;
	casLevel=RawTapFile[tapePos];
}

void UpdateTape(uint8_t cycles)
{
	if (!RawTapFile || tapePaused)
		return;

	tapeClock+=cycles*4096;
	if (tapeClock>=(TSTATES_PER_ROW*ROWS_PER_VBLANK*4096)/((44100)/50))
	{
		tapeClock-=(TSTATES_PER_ROW*ROWS_PER_VBLANK*4096)/((44100)/50);
		tapePos++;
		if (tapePos>RawTapLength)
			tapePos=0;

		casLevel=RawTapFile[tapePos];
	}
}

uint8_t GetTapeLevel()
{
	return casLevel;
}

void LoadSNA()
{
	uint32_t snapLength=0;
	uint8_t *snap=qLoad("lordsmid.sna",&snapLength);

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
	
	memcpy(Ram,&snap[27],32768+16384);

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

float tickCnt=0;
float tickRate=((TSTATES_PER_ROW*ROWS_PER_VBLANK*50)/44100.f);

/* audio ticked at same clock as everything else... so need a step down */
void UpdateAudio()
{
	tickCnt+=1.f;
	
	if (tickCnt>=tickRate)
	{
		tickCnt-=tickRate;

		if (amountAdded!=BUFFER_LEN)
		{
			int32_t res=0;
			res+=currentDAC[0];
			res+=tzxLevel?4095:-4096;
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


