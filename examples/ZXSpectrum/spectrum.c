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

void LoadTape();
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

	atStart=glfwGetTime();
#endif
	//////////////////

	LoadTape();
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

	while (1==1)
	{
		static int doDebug=0;
		PinSetPIN__INT(1);
		if (intClocks!=0)
		{
			PinSetPIN__INT(0);
			intClocks--;
		}

		PinSetPIN__CLK(1);

		if ((!PinGetPIN__M1()) && PinGetPIN__MREQ())		// NOT PERFECT, SINCE IT WILL CATCH INTERRUPT REQUESTS.. 
		{
			// First cycle of a new instruction
			if (doDebug)
				Disassemble(PinGetPIN_A(),1);
		}
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
			if (glfwGetKey(windows[MAIN_WINDOW],' '))
			{
				doDebug=1;
			}


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

void LoadTape()
{
	RawTapFile=qLoad("jsw.raw",&RawTapLength);
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

