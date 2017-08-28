/*
 * Simple Driver - to drive main schematic
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <GLFW/glfw3.h>
#include "glext.h"
#include "gui/debugger.h"

#if !defined(NDEBUG)
#define USE_TIMING 1
#else
#define USE_TIMING 0
#endif

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
extern uint16_t DIS_INS;

extern uint8_t IM;
extern uint8_t IFF1;
extern uint8_t IFF2;

#define TEST TESTULA

void TEST(uint8_t a);
void RESET1(uint8_t a);
void RESET2(uint8_t a);

extern uint8_t rom[16384];
extern uint8_t ram[16384];

#define WIDTH	(48+256+48+96)
#define	HEIGHT	(56+192+56+8)
#define TIMING_WIDTH	640*2
#define TIMING_HEIGHT	900

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define TIMING_WINDOW	1

GLFWwindow* windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

uint8_t *GetScreenPtr(uint32_t pixelAddress)
{
	uint32_t offset = pixelAddress & 0x181F;
	offset |= (pixelAddress & 0x700) >> 3;
	offset |= (pixelAddress & 0xE0) << 3;

	return &ram[offset];
}

uint8_t *GetAttrPtr(uint32_t pixelAddress)
{
	uint32_t offset = 0x1800 | (pixelAddress & 0x001F);
	offset |= (pixelAddress & 0x1F00) >> 3;

	return &ram[offset];
}

uint32_t GetColour(uint8_t attr, uint8_t pixSet,int flashTimer)
{
	uint32_t colour = 0;
	uint8_t ink = attr & 0x07;
	uint8_t paper = (attr & 0x38) >> 3;
	uint8_t bright = (attr & 0x40) >> 6;
	uint8_t flash = (attr & 0x80) >> 7;

	if (flash && (flashTimer & 0x10))
	{
		uint8_t swap = paper;
		paper = ink;
		ink = swap;
	}

	if (pixSet)
	{
		if (ink & 0x01)
		{
			if (bright)
				colour |= 0x000000FF;
			else
				colour |= 0x000000CC;
		}
		if (ink & 0x02)
		{
			if (bright)
				colour |= 0x00FF0000;
			else
				colour |= 0x00CC0000;
		}
		if (ink & 0x04)
		{
			if (bright)
				colour |= 0x0000FF00;
			else
				colour |= 0x0000CC00;
		}
	}
	else
	{
		if (paper & 0x01)
		{
			if (bright)
				colour |= 0x000000FF;
			else
				colour |= 0x000000CC;
		}
		if (paper & 0x02)
		{
			if (bright)
				colour |= 0x00FF0000;
			else
				colour |= 0x00CC0000;
		}
		if (paper & 0x04)
		{
			if (bright)
				colour |= 0x0000FF00;
			else
				colour |= 0x0000CC00;
		}
	}

	return colour;
}

uint8_t NOTBorderCol = 7;

int gy = 0;
int flashtimer = 0;

void UpdateScreen()
{
	gy = 0;
	flashtimer++;
}
void UpdateScreenLine()
{

//	for (int y = 0; y < 192; y++)
	if (gy<192)
	{
		int y = gy++;
		// Quick hack to draw the screen
		uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
		outputTexture += (WIDTH)*y;
		for (int x = 0; x < 256; x++)
		{
			{
				uint8_t* ptr = GetScreenPtr( (x) / 8 + y * 32);
				uint8_t* attr = GetAttrPtr( (x) / 8 + y * 32);
				uint8_t val = *ptr;
				uint8_t col = *attr;

				//val = rand();
				int ink;
				if (val & (0x80 >> (x % 8)))
					ink = 1;
				else
					ink = 0;
				*outputTexture = GetColour(col, ink, flashtimer);
			}
//			else
	//			*outputTexture = GetColour(NOTBorderCol, 0xFF, 0);
			outputTexture++;
		}
	}



	/*
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	numclocks<<=1;
	while (numclocks)
	{
		outputTexture[hClock+vClock*WIDTH]=GetColour(curAttrByte,curScrByte&scrXMask);
		scrXMask>>=1;
		
		// Handle fetch/prefetch
		//Check if we are reading this frame (sequence is 1 cpu clock : pixel byte , attribute byte, pixel byte, attribute byte, idle,idle,idle,idle)
		switch (hClock&15)
		{
			case 0:
				floatingBus=0xFF;
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
				break;
			case 7:
				curScrByte=nextScrByte2;
				curAttrByte=nextAttrByte2;
				scrXMask=0x80;
				break;
			case 8:
				if (isDisplayFetch())
				{
					nextScrByte1=*ulaScreenPtr;
					floatingBus=nextScrByte1;
				}
				else
				{
					nextScrByte1=0xFF;
					floatingBus=0xFF;
				}
				break;
			case 9:
				if (isDisplayFetch())
				{
					pixelAddress++;
				}
				ulaScreenPtr=GetScreenPtr(pixelAddress);
				break;
			case 10:
				if (isDisplayFetch())
				{
					nextAttrByte1=*ulaAttrPtr;
					floatingBus=nextAttrByte1;
				}
				else
				{
					nextAttrByte1=borderCol;
					floatingBus=0xFF;
				}
				break;
			case 11:
				ulaAttrPtr=GetAttrPtr(pixelAddress);
				break;
			case 12:
				if (isDisplayFetch())
				{
					nextScrByte2=*ulaScreenPtr;
					floatingBus=nextScrByte2;
				}
				else
				{
					nextScrByte2=0xFF;
					floatingBus=0xFF;
				}
				break;
			case 13:
				if (isDisplayFetch())
				{
					pixelAddress++;
				}
				ulaScreenPtr=GetScreenPtr(pixelAddress);
				break;
			case 14:
				if (isDisplayFetch())
				{
					nextAttrByte2=*ulaAttrPtr;
					floatingBus=nextAttrByte2;
				}
				else
				{
					nextAttrByte2=borderCol;
					floatingBus=0xFF;
				}
				break;
			case 15:
				ulaAttrPtr=GetAttrPtr(pixelAddress);
				scrXMask=0x80;
				curScrByte=nextScrByte1;
				curAttrByte=nextAttrByte1;
				break;
		}
		hClock++;
		if (hClock>=448)
		{
			hClock=0;
			vClock++;
		}
		numclocks--;
	}*/
}

void ShowScreen(int windowNum,int w,int h)
{
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f,1.0f);

	glTexCoord2f(0.0f, 0.0f+h);
	glVertex2f(-1.0f,-1.0f);

	glTexCoord2f(0.0f+w, 0.0f+h);
	glVertex2f(1.0f,-1.0f);

	glTexCoord2f(0.0f+w, 0.0f);
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
//	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_REPEAT);
//	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA, w,
			h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);

	glDisable(GL_DEPTH_TEST);
}


void sizeHandler(GLFWwindow* window, int xs, int ys)
{
	glfwMakeContextCurrent(window);
	glViewport(0, 0, xs, ys);
}

uint8_t GetByte(uint16_t address)
{
	if (address<0x4000)
	{
		return rom[address];
	}
	else
	{
		return ram[address&0x3FFF];
	}
}

void CPU_RESET()
{
	RESET2(0);
	for (int a = 0; a < 3 * 4; a++)
	{
		TEST(1);
		TEST(0);
	}
	RESET1(0);
}


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

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

	uint8_t byte = GetByte(address);
	if (byte>realLength)
	{
		snprintf(temporaryBuffer,sizeof(temporaryBuffer),"UNKNOWN OPCODE");
		return temporaryBuffer;
	}
	else
	{
       	const char* mnemonic=(const char*)table[byte];
	const char* sPtr=mnemonic;
	char* dPtr=temporaryBuffer;
	int counting = 0;
	int doingDecode=0;
	
	if (sPtr==NULL)
	{
		snprintf(temporaryBuffer,sizeof(temporaryBuffer),"UNKNOWN OPCODE");
		return temporaryBuffer;
	}

	if (strcmp(mnemonic,"CB")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_CB,address+1,&tmpCount,256);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"DD")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_DD,address+1,&tmpCount,250);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"DDCB")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_DDCB,address+2,&tmpCount,256);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"FDCB")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_FDCB,address+2,&tmpCount,256);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"ED")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_ED,address+1,&tmpCount,256);
		*count=tmpCount+1;
		return temporaryBuffer;
	}
	if (strcmp(mnemonic,"FD")==0)
	{
		int tmpCount=0;
		decodeDisasm(DIS_FD,address+1,&tmpCount,250);
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
			snprintf(sprintBuffer,sizeof(sprintBuffer),"%02X",GetByte(address+offset));
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
	const char* retVal = decodeDisasm(DIS_,address,&numBytes,256);

	if (strcmp(retVal,"UNKNOWN OPCODE")==0)
	{
		printf("UNKNOWN AT : %04X\n",address);
		for (a=0;a<numBytes+1;a++)
		{
			printf("%02X ",GetByte(address+a));
		}
		printf("\n");
		abort();
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

#define MAX_KEY_CODE	(512)
unsigned char keyArray[MAX_KEY_CODE * 3];

int KeyDown(int key)
{
	return keyArray[key * 3 + 1] == GLFW_PRESS;
}

int CheckKey(int key)
{
	return keyArray[key * 3 + 2];
}

void ClearKey(int key)
{
	keyArray[key * 3 + 2] = 0;
}

void kbHandler(GLFWwindow* window, int key, int scan, int action, int mod)		/* At present ignores which window, will add per window keys later */
{
	if (key<0 || key >= MAX_KEY_CODE)
		return;

	keyArray[key * 3 + 0] = keyArray[key * 3 + 1];
	if (action == GLFW_RELEASE)
	{
		keyArray[key * 3 + 1] = GLFW_RELEASE;
	}
	else
	{
		keyArray[key * 3 + 1] = GLFW_PRESS;
	}
	keyArray[key * 3 + 2] |= (keyArray[key * 3 + 0] == GLFW_RELEASE) && (keyArray[key * 3 + 1] == GLFW_PRESS);
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

int pause = 0;
int pauseframe = 0;

uint8_t GetTapeLevel();
void PauseTape();
void PlayTape();

uint8_t GetPort(uint16_t port)
{
	uint8_t matrixLine = ~(port >> 8);
	uint8_t retVal = 0x0;

	retVal |= CheckKeys(matrixLine & 0x80, GLFW_KEY_SPACE, GLFW_KEY_RIGHT_ALT, 'M', 'N', 'B');
	retVal |= CheckKeys(matrixLine & 0x40, GLFW_KEY_ENTER, 'L', 'K', 'J', 'H');
	retVal |= CheckKeys(matrixLine & 0x20, 'P', 'O', 'I', 'U', 'Y');
	retVal |= CheckKeys(matrixLine & 0x10, '0', '9', '8', '7', '6');
	retVal |= CheckKeys(matrixLine & 0x08, '1', '2', '3', '4', '5');
	retVal |= CheckKeys(matrixLine & 0x04, 'Q', 'W', 'E', 'R', 'T');
	retVal |= CheckKeys(matrixLine & 0x02, 'A', 'S', 'D', 'F', 'G');
	retVal |= CheckKeys(matrixLine & 0x01, GLFW_KEY_LEFT_SHIFT, 'Z', 'X', 'C', 'V');

	retVal = (~retVal) & 0x1F;

	//pauseframe = 1;

	retVal |= 0xA0;			// apparantly always logic 1
	if (GetTapeLevel() >= 0x80)
	{
		retVal |= 0x40;
	}
		
	return retVal;
}

void SetPort(uint16_t port, uint8_t data)
{
	NOTBorderCol = data & 7;
}

void SetVideo(uint16_t h, uint16_t v, uint32_t col)
{
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	outputTexture += WIDTH*v + h;
	*outputTexture = col;
}

int LoadRom(const char* romName, uint8_t* buffer, int size)
{
	FILE* rom = NULL;
	rom = fopen(romName, "rb");
	if (rom==NULL)
	{
		printf("Failed to load %s\n",romName);
		return 1;
	}
	fread(buffer, 1, size, rom);
	fclose(rom);
	return 0;
}

static void glfwError(int id, const char* description)
{
	printf("GLFW ERROR : [%d] %s",id,description);
}

int main(int argc,char**argv)
{
	uint16_t lastIns=0xAAAA;

	LoadTapeFormat(argv[1]);

	/// Initialize GLFW 
	glfwSetErrorCallback(&glfwError);
	glfwInit(); 

	// Open invaders OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwCreateWindow( WIDTH*2, HEIGHT*2, "spectrum",NULL,NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC
	int w, h;
	glfwGetWindowSize(windows[MAIN_WINDOW], &w, &h);
	sizeHandler(windows[MAIN_WINDOW], w, h);
	glfwSetWindowSizeCallback(windows[MAIN_WINDOW],sizeHandler);

	glfwSetKeyCallback(windows[MAIN_WINDOW], kbHandler);

#if USE_TIMING
	// Open timing OpenGL window 
	if (!(windows[TIMING_WINDOW] = glfwCreateWindow(TIMING_WIDTH, TIMING_HEIGHT, "Logic Analyser", NULL, NULL)))
	{
		glfwTerminate();
		return 1;
	}

	glfwSetWindowPos(windows[TIMING_WINDOW], 0, 640);

	glfwMakeContextCurrent(windows[TIMING_WINDOW]);
	setupGL(TIMING_WINDOW, TIMING_WIDTH, TIMING_HEIGHT);
	glfwSetWindowSizeCallback(windows[TIMING_WINDOW],sizeHandler);
#endif

	//if (LoadRom("roms/horacespiders.rom", rom, 16384))
	if (LoadRom("roms/zx.rom", rom, 16384))
	{
		return 1;
	}

	//if (LoadRom("c:/speccywork/next/zxnext/scr/test.scr", ram, 6912))
	//if (LoadRom("c:/speccywork/next/zxnext/scr/manicminer.scr", ram, 6912))
	if (LoadRom("scr/Fairlight.scr", ram, 6912))
	{
		return 1;
	}

/*	ram[0] = 0x00;
	ram[1] = 0x18;
	ram[2] = 0xFD;
	*/
	/*rom[0] = 0xF3;
	rom[1] = 0x21;
	rom[2] = 0x00;
	rom[3] = 0x40;
	rom[4] = 0x36;
	rom[5] = 0xAA;
	rom[6] = 0x18;
	rom[7] = 0xFC;*/
/*
	rom[0x66] = 0xF3;
	rom[0x67] = 0x36;
	rom[0x68] = 0x00;
	rom[0x69] = 0x7E;
	rom[0x6a] = 0x18;
	rom[0x6b] = 0xFD;*/


	CPU_RESET();
//	for (size_t a=0;a<640000*10;a++)
	int a=0;
	int b = 0;
	int c = 0;

	//PC = 0x4000;

	pauseframe = 1;
	while (1==1)
	{
		b++;
		c++;
		if (c >= 4 * 224)
		{
			//UpdateScreenLine();
			c -= 4 * 224;
		}
		if (b >= 4*224 * 312)
		{
			if (pauseframe)
			{
				pause = 1;
				pauseframe = 0;
			}
			b -= 4*224 * 312;

			//UpdateScreen();
            glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers(windows[MAIN_WINDOW]);

#if USE_TIMING
			glfwMakeContextCurrent(windows[TIMING_WINDOW]);
			DrawTiming(videoMemory[TIMING_WINDOW],TIMING_WIDTH,TIMING_HEIGHT);
			UpdateTimingWindow(windows[TIMING_WINDOW]);
			ShowScreen(TIMING_WINDOW,TIMING_WIDTH,TIMING_HEIGHT);
			glfwSwapBuffers(windows[TIMING_WINDOW]);
#endif
			if (glfwGetKey(windows[MAIN_WINDOW], GLFW_KEY_ESCAPE))
			{
				break;
			}
			if (CheckKey(GLFW_KEY_BACKSPACE))
			{
				ClearKey(GLFW_KEY_BACKSPACE);
				pause = !pause;
			}
			if (CheckKey(GLFW_KEY_PAGE_UP))
			{
				ClearKey(GLFW_KEY_PAGE_UP);
				PauseTape();
			}

			if (CheckKey(GLFW_KEY_PAGE_DOWN))
			{
				ClearKey(GLFW_KEY_PAGE_DOWN);
				PlayTape();
			}

			glfwPollEvents();
		}
		if (DIS_INS!=lastIns)
		{
			lastIns=DIS_INS;
/*			Disassemble(lastIns,1);
			if (lastIns == 0x11D0)//0x4001 && lastIns != 0x4002)
			{
				printf("");
				pause = 1;
			}*/
		}
		if (!pause)
		{
			UpdateTape( (a&3)==0?1:0 );
			TEST((a++) & 1);
			TEST((a++) & 1);
		}
	}

	return 0;
}

