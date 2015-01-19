/*
 * ZXSpectrum
 *
 * Common Functionality Between Pin Accurate Core and Simple Core
 *
 */

#include <GLFW/glfw3.h>
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
	if (LoadRom(0x0000,0x1000,"roms/zx80.rom"))
		return 1;
	return 0;
}

int masterClock=0;

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	256

#define TIMING_WIDTH	680
#define TIMING_HEIGHT	400

#define BORDERWIDTH	(16)

#define WIDTH	(256+48+96+48)
#define	HEIGHT	(64+192+56)

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0

GLFWwindow* windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

void ShowScreen(int windowNum,int w,int h)
{
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f+(96.f/WIDTH)*2,1.0f);

	glTexCoord2f(0.0f, h);
	glVertex2f(-1.0f+(96.f/WIDTH)*2,-1.0f);

	glTexCoord2f(w-96, h);
	glVertex2f(1.0f,-1.0f);

	glTexCoord2f(w-96, 0.0f);
	glVertex2f(1.0f,1.0f);
	
	glTexCoord2f(w-96, 0.0f-1);
	glVertex2f(-1.0f,1.0f);

	glTexCoord2f(w-96, h-1);
	glVertex2f(-1.0f,-1.0f);

	glTexCoord2f(w, h-1);
	glVertex2f(-1.0f+(96.f/WIDTH)*2,-1.0f);

	glTexCoord2f(w, 0.0f-1);
	glVertex2f(-1.0f+(96.f/WIDTH)*2,1.0f);

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

void kbHandler( GLFWwindow* window, int key, int scan, int action, int mod )		/* At present ignores which window, will add per window keys later */
{
	keyArray[key*3 + 0]=keyArray[key*3+1];
	if (action==GLFW_RELEASE)
	{
		keyArray[key*3 + 1]=GLFW_RELEASE;
	}
	else
	{
		keyArray[key*3 + 1]=GLFW_PRESS;
	}
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

uint8_t GetPort(uint16_t port)
{
	return 0xFF;
}

void SetPort(uint16_t port,uint8_t byte)
{
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
	const char* retVal = decodeDisasm(DIS_,address,&numBytes,256);

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

int intClocks=0;
void CPU_RESET();
int CPU_STEP(int intClocks,int doDebug);
	
int main(int argc,char**argv)
{
	double	atStart,now,remain;
	int a;
	int numClocks;

	/// Initialize GLFW 
	glfwInit(); 

	// Open invaders OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwCreateWindow( WIDTH, HEIGHT, "spectrum",NULL,NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC

	glfwSetKeyCallback(windows[MAIN_WINDOW],kbHandler);

	AudioInitialise();

	atStart=glfwGetTime();
	//////////////////

	if (InitialiseMemory())
		return -1;
	
	CPU_RESET();
	
//	DisassembleRange(0x0000,0x4000);

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

		//UpdateAudio(numClocks);
		
		if (masterClock>=99999)//ROWS_PER_VBLANK*TSTATES_PER_ROW)
		{	
//			ResetScreen();
			masterClock-=99999;//ROWS_PER_VBLANK*TSTATES_PER_ROW;

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers(windows[MAIN_WINDOW]);
				
			glfwPollEvents();
		
			if (glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESCAPE))
			{
				break;
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

	AudioKill();

	return 0;
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
//			res+=GetTapeLevel()?4095:-4096;
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

/*
 *  Uses Z80 step core built in EDL
 *  Rest is C for now
 */

#include <stdio.h>
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

uint8_t GetByte(uint16_t addr);
void SetByte(uint16_t addr,uint8_t byte);
uint8_t GetPort(uint16_t port);
void SetPort(uint16_t port,uint8_t byte);
int Disassemble(unsigned int address,int registers);

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

void CPU_RESET()
{
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
}


int CPU_STEP(int intClocks,int doDebug)
{
	if (intClocks)
	{
		PinSetPIN__INT(0);
	}
	else
	{
		PinSetPIN__INT(1);
	}

	PinSetPIN__CLK(1);
#if 1
	if ((!PinGetPIN__M1()) && PinGetPIN__MREQ())		// Catch all M1 cycles
	{
		// First cycle of a new instruction (DD,FD,CB,ED - all generate multiple m1 cycles - beware!)
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

	return 1;
}


