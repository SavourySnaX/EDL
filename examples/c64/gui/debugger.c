#include <GL/glfw3.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "font.h"

unsigned short breakpoints[2][20]={
	{0xDA,0},
	{/*0xE853,*//*0xF556,*/0xfd27,0xfd86,0xfd58,0xF497,0xF3C0,0xF567}
	};		// first list main cpu, second is disk cpu
unsigned int numBreakpoints[2]={1,0};

unsigned short curAddresses[2][16];
int Offs[2]={0,0};

void ToggleBreakPoint(int chip)
{
	int a,c;

	for (a=0;a<numBreakpoints[chip];a++)
	{
		if (curAddresses[chip][Offs[chip]]==breakpoints[chip][a])
		{
			for (c=a;c<numBreakpoints[chip];c++)
			{
				breakpoints[chip][c]=breakpoints[chip][c+1];
			}
			numBreakpoints[chip]--;
			return;
		}
	}

	breakpoints[chip][numBreakpoints[chip]]=curAddresses[chip][Offs[chip]];
	numBreakpoints[chip]++;
}

int isBreakpoint(int chip,uint16_t address)
{
	int a;
	for (a=0;a<numBreakpoints[chip];a++)
	{
		if (address==breakpoints[chip][a])
		{
			return 1;
		}
	}
	return 0;
}

void DrawChar(unsigned char* buffer,unsigned int width,unsigned int x,unsigned int y, char c,unsigned char r,unsigned char g,unsigned char b)
{
	unsigned int xx,yy;
	unsigned char *fontChar=&FontData[c*6*8];
	
	x*=8;
	y*=8;
	
	for (yy=0;yy<8;yy++)
	{
		for (xx=0;xx<6;xx++)
		{
			buffer[(x+xx+1 + (y+yy)*width)*4+0]=(*fontChar)*b;
			buffer[(x+xx+1 + (y+yy)*width)*4+1]=(*fontChar)*g;
			buffer[(x+xx+1 + (y+yy)*width)*4+2]=(*fontChar)*r;
			fontChar++;
		}
	}
}

void PrintAt(unsigned char* buffer,unsigned int width,unsigned char r,unsigned char g,unsigned char b,unsigned int x,unsigned int y,const char *msg,...)
{
	static char tStringBuffer[32768];
	char *pMsg=tStringBuffer;
	va_list args;

	va_start (args, msg);
	vsprintf (tStringBuffer,msg, args);
	va_end (args);

	while (*pMsg)
	{
		DrawChar(buffer,width,x,y,*pMsg,r,g,b);
		x++;
		pMsg++;
	}
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength);
const char* DISK_decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength);

extern uint8_t *MAIN_DIS_[256];

extern uint8_t	MAIN_A;
extern uint8_t	MAIN_X;
extern uint8_t	MAIN_Y;
extern uint16_t	MAIN_SP;
extern uint16_t	MAIN_PC;
extern uint8_t	MAIN_P;

extern uint8_t *DISK_DIS_[256];

extern uint8_t	DISK_A;
extern uint8_t	DISK_X;
extern uint8_t	DISK_Y;
extern uint16_t	DISK_SP;
extern uint16_t	DISK_PC;
extern uint8_t	DISK_P;

void DrawRegister(int chip,uint8_t *table[256],unsigned char* buffer,unsigned int width,uint16_t address,uint8_t (*GetMem)(uint16_t),const char *(*decode)(uint8_t *table[256],unsigned int address,int *count,int realLength))
{
	unsigned int b;
	int numBytes=0;
	
	for (b=0;b<16;b++)
	{
		int a;
		const char* retVal = decode(table,address,&numBytes,255);
		int opcodeLength=numBytes+1;
		unsigned char colR=255,colG=255,colB=255;

		curAddresses[chip][b]=address;
		if (isBreakpoint(chip,address))
		{
			colG=0;
			colB=0;
		}

		if (Offs[chip]==b)
		{
			PrintAt(buffer,width,colR,colG,colB,0,8+b,"%04X >",address);
		}
		else
		{
			PrintAt(buffer,width,colR,colG,colB,0,8+b,"%04X  ",address);
		}

		PrintAt(buffer,width,255,255,255,8,8+b,"            ");
		for (a=0;a<opcodeLength;a++)
		{
			PrintAt(buffer,width,colR,colG,colB,8+a*3,8+b,"%02X ",GetMem(address+a));
		}
		PrintAt(buffer,width,255,255,255,8+4*3,8+b,"            ");
		PrintAt(buffer,width,colR,colG,colB,8+3*3,8+b,"%s",retVal);

		address+=opcodeLength;
	}
}

extern uint8_t Ram[0x10000];

void DrawMemory(unsigned char* buffer,unsigned int width,uint16_t baseAddress,uint8_t (*GetMem)(uint16_t))
{
	int b;
	for (b=0;b<31;b++)
	{
		int a;
		unsigned char colR=255,colG=255,colB=255;
		PrintAt(buffer,width,colR,colG,colB,0,b,"%04X ",baseAddress+b*16);
		for (a=0;a<16;a++)
		{
			PrintAt(buffer,width,colR,colG,colB,6+a*3,b,"%02X ",GetMem(baseAddress + b*16+a));
		}
	}
}

void DrawRegisterDisk(unsigned char* buffer,unsigned int width,uint16_t address,uint8_t (*GetMem)(uint16_t))
{
	PrintAt(buffer,width,255,255,255,0,0,"FLAGS = N  V  -  B  D  I  Z  C");
	PrintAt(buffer,width,255,255,255,0,1,"        %s  %s  %s  %s  %s  %s  %s  %s",
		DISK_P&0x80 ? "1" : "0",
		DISK_P&0x40 ? "1" : "0",
		DISK_P&0x20 ? "1" : "0",
		DISK_P&0x10 ? "1" : "0",
		DISK_P&0x08 ? "1" : "0",
		DISK_P&0x04 ? "1" : "0",
		DISK_P&0x02 ? "1" : "0",
		DISK_P&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,2,"A   %02X",DISK_A);
	PrintAt(buffer,width,255,255,255,0,3,"X   %02X",DISK_X);
	PrintAt(buffer,width,255,255,255,0,4,"Y   %02X",DISK_Y);
	PrintAt(buffer,width,255,255,255,0,5,"SP  %04X",DISK_SP);

	DrawRegister(1,DISK_DIS_,buffer,width,address,GetMem,DISK_decodeDisasm);
}

void UpdateRegisterMain(GLFWwindow window)
{
	if (CheckKeyWindow(GLFW_KEY_DOWN,window))
	{
		Offs[0]++;
		if (Offs[0]>15)
		{
			Offs[0]=15;
		}
		ClearKey(GLFW_KEY_DOWN);
	}
	if (CheckKeyWindow(GLFW_KEY_UP,window))
	{
		Offs[0]--;
		if (Offs[0]<0)
		{
			Offs[0]=0;
		}
		ClearKey(GLFW_KEY_UP);
	}
	if (CheckKeyWindow(GLFW_KEY_SPACE,window))
	{
		ToggleBreakPoint(0);
		ClearKey(GLFW_KEY_SPACE);
	}
}

void UpdateRegisterDisk(GLFWwindow window)
{
	if (CheckKeyWindow(GLFW_KEY_DOWN,window))
	{
		Offs[1]++;
		if (Offs[1]>15)
		{
			Offs[1]=15;
		}
		ClearKey(GLFW_KEY_DOWN);
	}
	if (CheckKeyWindow(GLFW_KEY_UP,window))
	{
		Offs[1]--;
		if (Offs[1]<0)
		{
			Offs[1]=0;
		}
		ClearKey(GLFW_KEY_UP);
	}
	if (CheckKeyWindow(GLFW_KEY_SPACE,window))
	{
		ToggleBreakPoint(1);
		ClearKey(GLFW_KEY_SPACE);
	}
}

extern uint8_t M6569_Regs[0x40];

void DrawRegisterMain(unsigned char* buffer,unsigned int width,uint16_t address,uint8_t (*GetMem)(uint16_t))
{
	PrintAt(buffer,width,255,255,255,0,0,"FLAGS = N  V  -  B  D  I  Z  C");
	PrintAt(buffer,width,255,255,255,0,1,"        %s  %s  %s  %s  %s  %s  %s  %s",
		MAIN_P&0x80 ? "1" : "0",
		MAIN_P&0x40 ? "1" : "0",
		MAIN_P&0x20 ? "1" : "0",
		MAIN_P&0x10 ? "1" : "0",
		MAIN_P&0x08 ? "1" : "0",
		MAIN_P&0x04 ? "1" : "0",
		MAIN_P&0x02 ? "1" : "0",
		MAIN_P&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,2,"A   %02X",MAIN_A);
	PrintAt(buffer,width,255,255,255,0,3,"X   %02X",MAIN_X);
	PrintAt(buffer,width,255,255,255,0,4,"Y   %02X",MAIN_Y);
	PrintAt(buffer,width,255,255,255,0,5,"SP  %04X",MAIN_SP);

	DrawRegister(0,MAIN_DIS_,buffer,width,address,GetMem,decodeDisasm);
}

extern uint16_t VCBase;
extern uint16_t VC;
extern uint8_t	RC;

extern uint32_t spData[8];

void DrawRegister6569(unsigned char* buffer,unsigned int width)
{
	int a;

	PrintAt(buffer,width,255,255,255,0,0,"Sprites: On Y  X    Multi EX EY Data");
	for (a=0;a<8;a++)
	{
		PrintAt(buffer,width,255,255,255,9,2+a,"%s  %02X %04X %s     %s  %s %08X",
				M6569_Regs[0x15]&(1<<a)?"1":"0",
				M6569_Regs[0x01+a*2],
				M6569_Regs[0x00+a*2] + (M6569_Regs[0x10]&(1<<a)?0x100:0),
				M6569_Regs[0x1C]&(1<<a)?"1":"0",
				M6569_Regs[0x1D]&(1<<a)?"1":"0",
				M6569_Regs[0x17]&(1<<a)?"1":"0",
				spData[a]
		       );
	}


	PrintAt(buffer,width,255,255,255,0,12,"YSCROLL - %02X",M6569_Regs[0x11]&7);
	PrintAt(buffer,width,255,255,255,0,13,"XSCROLL - %02X",M6569_Regs[0x16]&7);
	
	PrintAt(buffer,width,255,255,255,0,15,"VCBASE  - %02X",VCBase);
	PrintAt(buffer,width,255,255,255,0,16,"VC      - %02X",VC);
	PrintAt(buffer,width,255,255,255,0,17,"RC      - %02X",RC);

	{
		int a;
		int b;

		for (a=0;a<8;a++)
		{
			for (b=0;b<8;b++)
			{
				PrintAt(buffer,width,255,255,255,0+b*3,26+a,"%02X",M6569_Regs[a*8+b]);
			}
		}
	}
}

uint8_t DISK_VIA0_PinGetPIN_PA();
uint8_t DISK_VIA0_PinGetPIN_CA1();
uint8_t DISK_VIA0_PinGetPIN_CA2();
uint8_t DISK_VIA0_PinGetPIN_PB();
uint8_t DISK_VIA0_PinGetPIN_CB1();
uint8_t DISK_VIA0_PinGetPIN_CB2();
extern uint8_t DISK_VIA0_DDRA;
extern uint8_t DISK_VIA0_DDRB;
uint8_t DISK_VIA1_PinGetPIN_PA();
uint8_t DISK_VIA1_PinGetPIN_CA1();
uint8_t DISK_VIA1_PinGetPIN_CA2();
uint8_t DISK_VIA1_PinGetPIN_PB();
uint8_t DISK_VIA1_PinGetPIN_CB1();
uint8_t DISK_VIA1_PinGetPIN_CB2();
extern uint8_t DISK_VIA1_DDRA;
extern uint8_t DISK_VIA1_DDRB;

uint8_t CIA0_PinGetPIN_PA();
uint8_t CIA0_PinGetPIN_PB();
uint8_t CIA1_PinGetPIN_PA();
uint8_t CIA1_PinGetPIN_PB();

extern uint8_t	CIA0_PRA;
extern uint8_t	CIA0_PRB;
extern uint8_t	CIA0_DDRA;
extern uint8_t	CIA0_DDRB;
extern uint16_t	CIA0_TA;
extern uint16_t	CIA0_TB;
extern uint16_t	CIA0_TA_LATCH;
extern uint16_t	CIA0_TB_LATCH;
extern uint8_t	CIA0_TOD_HR;
extern uint8_t	CIA0_TOD_MIN;
extern uint8_t	CIA0_TOD_SEC;
extern uint8_t	CIA0_TOD_10TH;
extern uint8_t	CIA0_TOD_LAT_HR;
extern uint8_t	CIA0_TOD_LAT_MIN;
extern uint8_t	CIA0_TOD_LAT_SEC;
extern uint8_t	CIA0_TOD_LAT_10TH;
extern uint8_t	CIA0_ALARM_HR;
extern uint8_t	CIA0_ALARM_MIN;
extern uint8_t	CIA0_ALARM_SEC;
extern uint8_t	CIA0_ALARM_10TH;
extern uint8_t	CIA0_SDR;
extern uint8_t	CIA0_ICR_MASK;
extern uint8_t	CIA0_ICR_DATA;
extern uint8_t	CIA0_CRA;
extern uint8_t	CIA0_CRB;

extern uint8_t	CIA1_PRA;
extern uint8_t	CIA1_PRB;
extern uint8_t	CIA1_DDRA;
extern uint8_t	CIA1_DDRB;
extern uint16_t	CIA1_TA;
extern uint16_t	CIA1_TB;
extern uint16_t	CIA1_TA_LATCH;
extern uint16_t	CIA1_TB_LATCH;
extern uint8_t	CIA1_TOD_HR;
extern uint8_t	CIA1_TOD_MIN;
extern uint8_t	CIA1_TOD_SEC;
extern uint8_t	CIA1_TOD_10TH;
extern uint8_t	CIA1_TOD_LAT_HR;
extern uint8_t	CIA1_TOD_LAT_MIN;
extern uint8_t	CIA1_TOD_LAT_SEC;
extern uint8_t	CIA1_TOD_LAT_10TH;
extern uint8_t	CIA1_ALARM_HR;
extern uint8_t	CIA1_ALARM_MIN;
extern uint8_t	CIA1_ALARM_SEC;
extern uint8_t	CIA1_ALARM_10TH;
extern uint8_t	CIA1_SDR;
extern uint8_t	CIA1_ICR_MASK;
extern uint8_t	CIA1_ICR_DATA;
extern uint8_t	CIA1_CRA;
extern uint8_t	CIA1_CRB;


extern uint8_t	DISK_VIA0_PA_LATCH;
extern uint8_t	DISK_VIA0_PB_LATCH;
extern uint8_t	DISK_VIA0_ORA;
extern uint8_t	DISK_VIA0_ORB;
extern uint16_t DISK_VIA0_T1C;
extern uint16_t DISK_VIA0_T1L;
extern uint16_t DISK_VIA0_T2C;
extern uint8_t	DISK_VIA0_T2LL;
extern uint8_t	DISK_VIA0_SR;
extern uint8_t	DISK_VIA0_ACR;
extern uint8_t	DISK_VIA0_PCR;
extern uint8_t	DISK_VIA0_IFR;
extern uint8_t	DISK_VIA0_IER;

extern uint8_t	DISK_VIA1_PA_LATCH;
extern uint8_t	DISK_VIA1_PB_LATCH;
extern uint8_t	DISK_VIA1_ORA;
extern uint8_t	DISK_VIA1_ORB;
extern uint16_t DISK_VIA1_T1C;
extern uint16_t DISK_VIA1_T1L;
extern uint16_t DISK_VIA1_T2C;
extern uint8_t	DISK_VIA1_T2LL;
extern uint8_t	DISK_VIA1_SR;
extern uint8_t	DISK_VIA1_ACR;
extern uint8_t	DISK_VIA1_PCR;
extern uint8_t	DISK_VIA1_IFR;
extern uint8_t	DISK_VIA1_IER;

void DrawVIAMain(unsigned char* buffer,unsigned int width)
{
	PrintAt(buffer,width,255,255,255,0,0,"-----------------CIA#1 (DC00)");
	PrintAt(buffer,width,255,255,255,0,2,"PA = RW7 OT  RW6 OT  RW5 OT  RW4 OT  RW3 OT  RW2 OT  RW1 OT  RW0 OT");
	PrintAt(buffer,width,255,255,255,0,3,"     %s       %s       %s       %s        %s       %s       %s       %s",
		CIA0_PinGetPIN_PA()&0x80 ? "1" : "0",
		CIA0_PinGetPIN_PA()&0x40 ? "1" : "0",
		CIA0_PinGetPIN_PA()&0x20 ? "1" : "0",
		CIA0_PinGetPIN_PA()&0x10 ? "1" : "0",
		CIA0_PinGetPIN_PA()&0x08 ? "1" : "0",
		CIA0_PinGetPIN_PA()&0x04 ? "1" : "0",
		CIA0_PinGetPIN_PA()&0x02 ? "1" : "0",
		CIA0_PinGetPIN_PA()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,4,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		CIA0_DDRA&0x80 ? "1" : "0",
		CIA0_DDRA&0x40 ? "1" : "0",
		CIA0_DDRA&0x20 ? "1" : "0",
		CIA0_DDRA&0x10 ? "1" : "0",
		CIA0_DDRA&0x08 ? "1" : "0",
		CIA0_DDRA&0x04 ? "1" : "0",
		CIA0_DDRA&0x02 ? "1" : "0",
		CIA0_DDRA&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,5,"PB = CL7 IN  CL6 IN  CL5 IN  CL4 IN  CL3 IN  CL2 IN  CL1 IN  CL0 IN");
	PrintAt(buffer,width,255,255,255,0,6,"     %s       %s       %s       %s        %s       %s       %s       %s",
		CIA0_PinGetPIN_PB()&0x80 ? "1" : "0",
		CIA0_PinGetPIN_PB()&0x40 ? "1" : "0",
		CIA0_PinGetPIN_PB()&0x20 ? "1" : "0",
		CIA0_PinGetPIN_PB()&0x10 ? "1" : "0",
		CIA0_PinGetPIN_PB()&0x08 ? "1" : "0",
		CIA0_PinGetPIN_PB()&0x04 ? "1" : "0",
		CIA0_PinGetPIN_PB()&0x02 ? "1" : "0",
		CIA0_PinGetPIN_PB()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,7,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		CIA0_DDRB&0x80 ? "1" : "0",
		CIA0_DDRB&0x40 ? "1" : "0",
		CIA0_DDRB&0x20 ? "1" : "0",
		CIA0_DDRB&0x10 ? "1" : "0",
		CIA0_DDRB&0x08 ? "1" : "0",
		CIA0_DDRB&0x04 ? "1" : "0",
		CIA0_DDRB&0x02 ? "1" : "0",
		CIA0_DDRB&0x01 ? "1" : "0");

	PrintAt(buffer,width,255,255,255,0,9,"PRA  PRB  TA    TB    TAL   TBL   TOD       IFR  IER  CRA  CRB");
	PrintAt(buffer,width,255,255,255,0,10,"%02X   %02X   %04X  %04X  %04X  %04X  %02X%02X%02X%02X  %02X   %02X   %02X   %02X",
		CIA0_PRA,
		CIA0_PRB,
		CIA0_TA,
		CIA0_TB,
		CIA0_TA_LATCH,
		CIA0_TB_LATCH,
		CIA0_TOD_HR,
		CIA0_TOD_MIN,
		CIA0_TOD_SEC,
		CIA0_TOD_10TH,
		CIA0_ICR_MASK,
		CIA0_ICR_DATA,
		CIA0_CRA,
		CIA0_CRB
	       );


	PrintAt(buffer,width,255,255,255,0,12,"-----------------CIA#2(DD00)");
	PrintAt(buffer,width,255,255,255,0,14,"PA = RW7 IN  RW6 IN  RW5 IN  RW4 IN  RW3 IN  RW2 IN  RW1 IN  RW0 IN");
	PrintAt(buffer,width,255,255,255,0,15,"     %s       %s       %s       %s        %s       %s       %s       %s",
		CIA1_PinGetPIN_PA()&0x80 ? "1" : "0",
		CIA1_PinGetPIN_PA()&0x40 ? "1" : "0",
		CIA1_PinGetPIN_PA()&0x20 ? "1" : "0",
		CIA1_PinGetPIN_PA()&0x10 ? "1" : "0",
		CIA1_PinGetPIN_PA()&0x08 ? "1" : "0",
		CIA1_PinGetPIN_PA()&0x04 ? "1" : "0",
		CIA1_PinGetPIN_PA()&0x02 ? "1" : "0",
		CIA1_PinGetPIN_PA()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,16,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		CIA1_DDRA&0x80 ? "1" : "0",
		CIA1_DDRA&0x40 ? "1" : "0",
		CIA1_DDRA&0x20 ? "1" : "0",
		CIA1_DDRA&0x10 ? "1" : "0",
		CIA1_DDRA&0x08 ? "1" : "0",
		CIA1_DDRA&0x04 ? "1" : "0",
		CIA1_DDRA&0x02 ? "1" : "0",
		CIA1_DDRA&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,17,"PB = CL7 OT  CL6 OT  CL5 OT  CL4 OT  CL3 OT  CL2 OT  CL1 OT  CL0 OT");
	PrintAt(buffer,width,255,255,255,0,18,"     %s       %s       %s       %s        %s       %s       %s       %s",
		CIA1_PinGetPIN_PB()&0x80 ? "1" : "0",
		CIA1_PinGetPIN_PB()&0x40 ? "1" : "0",
		CIA1_PinGetPIN_PB()&0x20 ? "1" : "0",
		CIA1_PinGetPIN_PB()&0x10 ? "1" : "0",
		CIA1_PinGetPIN_PB()&0x08 ? "1" : "0",
		CIA1_PinGetPIN_PB()&0x04 ? "1" : "0",
		CIA1_PinGetPIN_PB()&0x02 ? "1" : "0",
		CIA1_PinGetPIN_PB()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,19,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		CIA1_DDRB&0x80 ? "1" : "0",
		CIA1_DDRB&0x40 ? "1" : "0",
		CIA1_DDRB&0x20 ? "1" : "0",
		CIA1_DDRB&0x10 ? "1" : "0",
		CIA1_DDRB&0x08 ? "1" : "0",
		CIA1_DDRB&0x04 ? "1" : "0",
		CIA1_DDRB&0x02 ? "1" : "0",
		CIA1_DDRB&0x01 ? "1" : "0");
	
	PrintAt(buffer,width,255,255,255,0,21,"PRA  PRB  TA    TB    TAL   TBL   TOD       IFR  IER  CRA  CRB");
	PrintAt(buffer,width,255,255,255,0,22,"%02X   %02X   %04X  %04X  %04X  %04X  %02X%02X%02X%02X  %02X   %02X   %02X   %02X",
		CIA1_PRA,
		CIA1_PRB,
		CIA1_TA,
		CIA1_TB,
		CIA1_TA_LATCH,
		CIA1_TB_LATCH,
		CIA1_TOD_HR,
		CIA1_TOD_MIN,
		CIA1_TOD_SEC,
		CIA1_TOD_10TH,
		CIA1_ICR_MASK,
		CIA1_ICR_DATA,
		CIA1_CRA,
		CIA1_CRB
	       );
}

void DrawVIADisk(unsigned char* buffer,unsigned int width)
{
	PrintAt(buffer,width,255,255,255,0,0,"-----------------VIA 1(1800)");
	PrintAt(buffer,width,255,255,255,0,2,"PA = PA7     PA6     PA5     PA4     PA3     PA2     PA1     PA0      CA1  ???  ?? = %s ",
		DISK_VIA0_PinGetPIN_CA1()&0x80 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,3,"     %s       %s       %s       %s        %s       %s       %s       %s       CA2  ???  ?? = %s",
		DISK_VIA0_PinGetPIN_PA()&0x80 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PA()&0x40 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PA()&0x20 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PA()&0x10 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PA()&0x08 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PA()&0x04 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PA()&0x02 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PA()&0x01 ? "1" : "0",
		DISK_VIA0_PinGetPIN_CA2()&0x80 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,4,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA0_DDRA&0x80 ? "1" : "0",
		DISK_VIA0_DDRA&0x40 ? "1" : "0",
		DISK_VIA0_DDRA&0x20 ? "1" : "0",
		DISK_VIA0_DDRA&0x10 ? "1" : "0",
		DISK_VIA0_DDRA&0x08 ? "1" : "0",
		DISK_VIA0_DDRA&0x04 ? "1" : "0",
		DISK_VIA0_DDRA&0x02 ? "1" : "0",
		DISK_VIA0_DDRA&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,5,"PB = ATN IN  DA1 IN  DA0 IN  ATN OT  CLK OT  CLK IN  DAT OT  DAT IN   CB1  ???  ?? = %s",
		DISK_VIA0_PinGetPIN_CB1()&0x80 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,6,"     %s       %s       %s       %s        %s       %s       %s       %s       CB2  ATN  IN = %s",
		DISK_VIA0_PinGetPIN_PB()&0x80 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PB()&0x40 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PB()&0x20 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PB()&0x10 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PB()&0x08 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PB()&0x04 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PB()&0x02 ? "1" : "0",
		DISK_VIA0_PinGetPIN_PB()&0x01 ? "1" : "0",
		DISK_VIA0_PinGetPIN_CB2()&0x80 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,7,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA0_DDRB&0x80 ? "1" : "0",
		DISK_VIA0_DDRB&0x40 ? "1" : "0",
		DISK_VIA0_DDRB&0x20 ? "1" : "0",
		DISK_VIA0_DDRB&0x10 ? "1" : "0",
		DISK_VIA0_DDRB&0x08 ? "1" : "0",
		DISK_VIA0_DDRB&0x04 ? "1" : "0",
		DISK_VIA0_DDRB&0x02 ? "1" : "0",
		DISK_VIA0_DDRB&0x01 ? "1" : "0");
	
	PrintAt(buffer,width,255,255,255,0,9,"PA_LAT  PB_LAT  ORA  ORB  T1C   T1L   T2C   T2L   SR   ACR  PCR  IFR  IER");
	PrintAt(buffer,width,255,255,255,0,10,"%02X      %02X      %02X   %02X   %04X  %04X  %04X  %02X    %02X   %02X   %02X   %02X   %02X",
		DISK_VIA0_PA_LATCH,
		DISK_VIA0_PB_LATCH,
		DISK_VIA0_ORA,
		DISK_VIA0_ORB,
		DISK_VIA0_T1C,
		DISK_VIA0_T1L,
		DISK_VIA0_T2C,
		DISK_VIA0_T2LL,
		DISK_VIA0_SR,
		DISK_VIA0_ACR,
		DISK_VIA0_PCR,
		DISK_VIA0_IFR,
		DISK_VIA0_IER
	       );

	PrintAt(buffer,width,255,255,255,0,12,"-----------------VIA 2(1C00)");
	PrintAt(buffer,width,255,255,255,0,14,"PA = PA7     PA6     PA5     PA4     PA3     PA2     PA1     PA0      CA1  BYT  IN = %s",
		DISK_VIA1_PinGetPIN_CA1()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,15,"     %s       %s       %s       %s        %s       %s       %s       %s       CA2  SOE  OT = %s",
		DISK_VIA1_PinGetPIN_PA()&0x80 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x40 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x20 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x10 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x08 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x04 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x02 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x01 ? "1" : "0",
		DISK_VIA1_PinGetPIN_CA2()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,16,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA1_DDRA&0x80 ? "1" : "0",
		DISK_VIA1_DDRA&0x40 ? "1" : "0",
		DISK_VIA1_DDRA&0x20 ? "1" : "0",
		DISK_VIA1_DDRA&0x10 ? "1" : "0",
		DISK_VIA1_DDRA&0x08 ? "1" : "0",
		DISK_VIA1_DDRA&0x04 ? "1" : "0",
		DISK_VIA1_DDRA&0x02 ? "1" : "0",
		DISK_VIA1_DDRA&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,17,"PB = !SY IN  CK1 OT  CK0 OT  !WP IN  LED OT  ON  OT  SL1 OT  SL0 OT   CB1  ???  ?? = %s",
		DISK_VIA1_PinGetPIN_CB1()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,18,"     %s       %s       %s       %s        %s       %s       %s       %s       CB2  ???  ?? = %s",
		DISK_VIA1_PinGetPIN_PB()&0x80 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x40 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x20 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x10 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x08 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x04 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x02 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x01 ? "1" : "0",
		DISK_VIA1_PinGetPIN_CB2()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,19,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA1_DDRB&0x80 ? "1" : "0",
		DISK_VIA1_DDRB&0x40 ? "1" : "0",
		DISK_VIA1_DDRB&0x20 ? "1" : "0",
		DISK_VIA1_DDRB&0x10 ? "1" : "0",
		DISK_VIA1_DDRB&0x08 ? "1" : "0",
		DISK_VIA1_DDRB&0x04 ? "1" : "0",
		DISK_VIA1_DDRB&0x02 ? "1" : "0",
		DISK_VIA1_DDRB&0x01 ? "1" : "0");

	PrintAt(buffer,width,255,255,255,0,21,"PA_LAT  PB_LAT  ORA  ORB  T1C   T1L   T2C   T2L   SR   ACR  PCR  IFR  IER");
	PrintAt(buffer,width,255,255,255,0,22,"%02X      %02X      %02X   %02X   %04X  %04X  %04X  %02X    %02X   %02X   %02X   %02X   %02X",
		DISK_VIA1_PA_LATCH,
		DISK_VIA1_PB_LATCH,
		DISK_VIA1_ORA,
		DISK_VIA1_ORB,
		DISK_VIA1_T1C,
		DISK_VIA1_T1L,
		DISK_VIA1_T2C,
		DISK_VIA1_T2LL,
		DISK_VIA1_SR,
		DISK_VIA1_ACR,
		DISK_VIA1_PCR,
		DISK_VIA1_IFR,
		DISK_VIA1_IER
	       );

}

#define MAX_CAPTURE		(65536)
#define MAX_PINS		(8)
#define MAX_PINS_A		(9)

unsigned char alohi[MAX_PINS_A][MAX_CAPTURE];
unsigned char lohi[MAX_PINS][MAX_CAPTURE];

int bufferPosition=0;
uint32_t timeStretch=0x10000;

void RecordPin(int pinPos,uint8_t pinVal)
{
	lohi[pinPos][bufferPosition]=pinVal&1;
}

void RecordPinA(int pinPos,uint8_t pinVal)
{
	alohi[pinPos][bufferPosition]=pinVal;
}

void UpdatePinTick()
{
	bufferPosition++;
	if (bufferPosition>=MAX_CAPTURE)
		bufferPosition=0;
}

void DrawTiming(unsigned char* buffer,unsigned int width)
{
	int a,b;
	unsigned int pulsepos;
	unsigned int prevpulsepos;

	PrintAt(buffer,width,255,255,255,0,0,"ATN (BUS)");
	PrintAt(buffer,width,255,255,255,0,3,"CLK (BUS)");
	PrintAt(buffer,width,255,255,255,0,6,"DAT (BUS)");
	PrintAt(buffer,width,255,255,255,0,9,"CLK (DISK)");
	PrintAt(buffer,width,255,255,255,0,12,"DAT (DISK)");
	PrintAt(buffer,width,255,255,255,0,15,"ATN (DISK)");
	PrintAt(buffer,width,255,255,255,0,18,"DISK IN");
	PrintAt(buffer,width,255,255,255,0,21,"CAS ");
//	PrintAt(buffer,width,255,255,255,0,24,"SID 1");
//	PrintAt(buffer,width,255,255,255,0,27,"SID 2");
//	PrintAt(buffer,width,255,255,255,0,30,"SID 3");

	pulsepos=bufferPosition<<16;
	prevpulsepos=bufferPosition<<16;

	for (a=0;a<MAX_PINS;a++)
	{
		for (b=0;b<512;b++)
		{
			if (lohi[a][pulsepos>>16])
			{
				buffer[(80+b+(a*8*3+5)*width)*4+0]=0xFF;
				buffer[(80+b+(a*8*3+5)*width)*4+1]=0xFF;
				buffer[(80+b+(a*8*3+5)*width)*4+2]=0xFF;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+0]=0x00;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+1]=0x00;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+2]=0x00;
			}
			else
			{
				buffer[(80+b+(a*8*3+5)*width)*4+0]=0x00;
				buffer[(80+b+(a*8*3+5)*width)*4+1]=0x00;
				buffer[(80+b+(a*8*3+5)*width)*4+2]=0x00;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+0]=0xFF;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+1]=0xFF;
				buffer[(80+b+(a*8*3+8*2+1)*width)*4+2]=0xFF;
			}

			if (b!=0 && lohi[a][prevpulsepos>>16]!=lohi[a][pulsepos>>16])
			{
				int c;
				for (c=6;c<8*2+1;c++)
				{
					buffer[(80+b+(a*8*3+c)*width)*4+0]=0xFF;
					buffer[(80+b+(a*8*3+c)*width)*4+1]=0xFF;
					buffer[(80+b+(a*8*3+c)*width)*4+2]=0xFF;
				}
			}
			else
			{
				int c;
				for (c=6;c<8*2+1;c++)
				{
					buffer[(80+b+(a*8*3+c)*width)*4+0]=0x00;
					buffer[(80+b+(a*8*3+c)*width)*4+1]=0x00;
					buffer[(80+b+(a*8*3+c)*width)*4+2]=0x00;
				}
			}

			prevpulsepos=pulsepos;
			pulsepos+=timeStretch;
			if ((pulsepos>>16)>=MAX_CAPTURE)
			{
				pulsepos=0;
			}
		}
	}
}

uint32_t cols[MAX_PINS_A]={0x00FF0000,0x0000FF00,0x000000FF,0x00880088,0x00888800,0x00008888,0x00FF0088,0x0000FF88,0x000088FF};

int pinOn[MAX_PINS_A]={1,1,1,1,1,1,1,1,1};

void DrawTimingA(unsigned char* buffer,unsigned int width)
{
	int a,b,c;
	int pulsepos;
	int pulsestart;
	uint32_t* argb=(uint32_t*)buffer;

	if (pinOn[0])
	{
		PrintAt(buffer,width,(cols[0]>>16)&0xFF,(cols[0]>>8)&0xFF,(cols[0]>>0)&0xFF,0,0,"OSC 1");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,0,"    ");
	}
	if (pinOn[1])
	{
		PrintAt(buffer,width,(cols[1]>>16)&0xFF,(cols[1]>>8)&0xFF,(cols[1]>>0)&0xFF,0,1,"OSC 2");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,1,"    ");
	}
	if (pinOn[2])
	{
		PrintAt(buffer,width,(cols[2]>>16)&0xFF,(cols[2]>>8)&0xFF,(cols[2]>>0)&0xFF,0,2,"OSC 3");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,2,"    ");
	}
	if (pinOn[3])
	{
		PrintAt(buffer,width,(cols[3]>>16)&0xFF,(cols[3]>>8)&0xFF,(cols[3]>>0)&0xFF,0,3,"ENV 1");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,3,"    ");
	}
	if (pinOn[4])
	{
		PrintAt(buffer,width,(cols[4]>>16)&0xFF,(cols[4]>>8)&0xFF,(cols[4]>>0)&0xFF,0,4,"ENV 2");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,4,"    ");
	}
	if (pinOn[5])
	{
		PrintAt(buffer,width,(cols[5]>>16)&0xFF,(cols[5]>>8)&0xFF,(cols[5]>>0)&0xFF,0,5,"ENV 3");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,5,"    ");
	}
	if (pinOn[6])
	{
		PrintAt(buffer,width,(cols[6]>>16)&0xFF,(cols[6]>>8)&0xFF,(cols[6]>>0)&0xFF,0,6,"CMB 1");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,6,"    ");
	}
	if (pinOn[7])
	{
		PrintAt(buffer,width,(cols[7]>>16)&0xFF,(cols[7]>>8)&0xFF,(cols[7]>>0)&0xFF,0,7,"CMB 2");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,7,"    ");
	}
	if (pinOn[8])
	{
		PrintAt(buffer,width,(cols[8]>>16)&0xFF,(cols[8]>>8)&0xFF,(cols[8]>>0)&0xFF,0,8,"CMB 3");
	}
	else
	{
		PrintAt(buffer,width,0,0,0,0,8,"    ");
	}

	pulsepos=(bufferPosition-(512*2-1))<<16;
	if ((pulsepos>>16)<0)
	{
		pulsepos+=MAX_CAPTURE<<16;
	}
	pulsestart=pulsepos;
	
	// Clear graph space
	
	for (b=0;b<512*2;b++)
	{
		for (c=12;c<256+12;c++)
		{
			argb[80+b+c*width]=0;
		}
	}

	uint32_t level;
	for (a=0;a<MAX_PINS_A;a++)
	{
		pulsepos=pulsestart;
		if (pinOn[a])
		{
			for (b=0;b<512*2;b++)
			{
				level = 12+ (255-(alohi[a][pulsepos>>16]));
				argb[80+b+level*width]=cols[a];

				pulsepos+=timeStretch;
				if ((pulsepos>>16)>=MAX_CAPTURE)
				{
					pulsepos=0;
				}
			}
		}
	}
}

void UpdateTimingWindow(GLFWwindow window)
{
	int a;

	if (CheckKeyWindow(GLFW_KEY_DOWN,window))
	{
		if (timeStretch>1)
			timeStretch>>=1;
		ClearKey(GLFW_KEY_DOWN);
	}
	if (CheckKeyWindow(GLFW_KEY_UP,window))
	{
		if (timeStretch<0x80000000)
			timeStretch<<=1;
		ClearKey(GLFW_KEY_UP);
	}

	for (a=0;a<MAX_PINS_A;a++)
	{
		if (CheckKeyWindow('0'+a,window))
		{
			pinOn[a]^=1;
			ClearKey('0'+a);
		}
	}
}

