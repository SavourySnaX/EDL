#include <GL/glfw3.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "font.h"

unsigned short breakpoints[20]={0x0067};
unsigned int numBreakpoints=0;

int isBreakpoint(unsigned int address)
{
	int a;
	for (a=0;a<numBreakpoints;a++)
	{
		if (address==breakpoints[a])
			return 1;
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

void DrawRegister(uint8_t *table[256],unsigned char* buffer,unsigned int width,uint16_t address,uint8_t (*GetMem)(uint16_t),const char *(*decode)(uint8_t *table[256],unsigned int address,int *count,int realLength))
{
	unsigned int b;
	int numBytes=0;
	
	for (b=0;b<16;b++)
	{
		int a;
		const char* retVal = decode(table,address,&numBytes,255);
		int opcodeLength=numBytes+1;
		unsigned char colR=255,colG=255,colB=255;

		for (a=0;a<numBreakpoints;a++)
		{
			if (address==breakpoints[a])
			{
				colG=0;
				colB=0;
			}
		}

		PrintAt(buffer,width,colR,colG,colB,0,10+b,"%04X  ",address);

		PrintAt(buffer,width,255,255,255,8,10+b,"            ");
		for (a=0;a<opcodeLength;a++)
		{
			PrintAt(buffer,width,colR,colG,colB,8+a*3,10+b,"%02X ",GetMem(address+a));
		}
		PrintAt(buffer,width,255,255,255,8+4*3,10+b,"            ");
		PrintAt(buffer,width,colR,colG,colB,8+4*3,10+b,"%s",retVal);

		address+=opcodeLength;
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

	DrawRegister(DISK_DIS_,buffer,width,address,GetMem,DISK_decodeDisasm);
}

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

	DrawRegister(MAIN_DIS_,buffer,width,address,GetMem,decodeDisasm);
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
uint8_t VIA0_PinGetPIN_PA();
uint8_t VIA0_PinGetPIN_CA1();
uint8_t VIA0_PinGetPIN_CA2();
uint8_t VIA0_PinGetPIN_PB();
uint8_t VIA0_PinGetPIN_CB1();
uint8_t VIA0_PinGetPIN_CB2();
extern uint8_t VIA0_DDRA;
extern uint8_t VIA0_DDRB;
uint8_t VIA1_PinGetPIN_PA();
uint8_t VIA1_PinGetPIN_CA1();
uint8_t VIA1_PinGetPIN_CA2();
uint8_t VIA1_PinGetPIN_PB();
uint8_t VIA1_PinGetPIN_CB1();
uint8_t VIA1_PinGetPIN_CB2();
extern uint8_t VIA1_DDRA;
extern uint8_t VIA1_DDRB;

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
//	Serial BUS				Motor And READ/WRITE
//	VIA 1	1800-180F			VIA 2	1C00-1C0F
//	NMI??					IRQ??
//	CA1 - ????				CA1	Byte Ready??
// 	PA0 - 					PA0	DATA TO/FROM DISK HEAD
//	PA1 - 					PA1	DATA TO/FROM DISK HEAD
//	PA2 - 					PA2	DATA TO/FROM DISK HEAD
//	PA3 - 					PA3	DATA TO/FROM DISK HEAD
//	PA4 - 					PA4	DATA TO/FROM DISK HEAD
//	PA5 - 					PA5	DATA TO/FROM DISK HEAD
//	PA6 - 					PA6	DATA TO/FROM DISK HEAD
//	PA7 - 					PA7	DATA TO/FROM DISK HEAD
//	CA2 - ????				CA2	SOE - on DISK_6502
//
//	CB1 - ????				CB1	???
//	PB0 - DATA IN				PB0	STEP MOTOR
//	PB1 - DATA OUT				PB1	STEP MOTOR
//	PB2 - CLOCK IN				PB2	MTR / drive motor (on/off)
//	PB3 - CLOCK OUT				PB3	ACT / LED on drive
//	PB4 - ATN ACK OUT			PB4	Write Protect Sense (1==on)
//	PB5 - DEVICE ADDRESS			PB5	BIT RATE
//	PB6 - DEVICE ADDRESS			PB6	BIT RATE
//	PB7 - ATN IN				PB7	SYNC IN
//	//CB2 - ATN IN				CB2	????
//
//
//

void DrawVIA(unsigned char* buffer,unsigned int width)
{
	PrintAt(buffer,width,255,255,255,0,0,"-----------------VIA 1(9110) (vic20)");
	PrintAt(buffer,width,255,255,255,0,1,"PA = ATN OT  CSW IN  PEN IN  JY2 IN  JY1 IN  JY0 IN  DAT IN  CLK IN   CA1  !RST OT = %s",
		VIA0_PinGetPIN_CA1()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,2,"     %s       %s       %s       %s        %s       %s       %s       %s",
		VIA0_PinGetPIN_PA()&0x80 ? "1" : "0",
		VIA0_PinGetPIN_PA()&0x40 ? "1" : "0",
		VIA0_PinGetPIN_PA()&0x20 ? "1" : "0",
		VIA0_PinGetPIN_PA()&0x10 ? "1" : "0",
		VIA0_PinGetPIN_PA()&0x08 ? "1" : "0",
		VIA0_PinGetPIN_PA()&0x04 ? "1" : "0",
		VIA0_PinGetPIN_PA()&0x02 ? "1" : "0",
		VIA0_PinGetPIN_PA()&0x01 ? "1" : "0"
		);
	PrintAt(buffer,width,255,255,255,0,3,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		VIA0_DDRA&0x80 ? "1" : "0",
		VIA0_DDRA&0x40 ? "1" : "0",
		VIA0_DDRA&0x20 ? "1" : "0",
		VIA0_DDRA&0x10 ? "1" : "0",
		VIA0_DDRA&0x08 ? "1" : "0",
		VIA0_DDRA&0x04 ? "1" : "0",
		VIA0_DDRA&0x02 ? "1" : "0",
		VIA0_DDRA&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,4,"PB = USR7    USR6    USR5    USR4    USR3    USR2    USR1    USR0   ");
	PrintAt(buffer,width,255,255,255,0,5,"     %s       %s       %s       %s        %s       %s       %s       %s",
		VIA0_PinGetPIN_PB()&0x80 ? "1" : "0",
		VIA0_PinGetPIN_PB()&0x40 ? "1" : "0",
		VIA0_PinGetPIN_PB()&0x20 ? "1" : "0",
		VIA0_PinGetPIN_PB()&0x10 ? "1" : "0",
		VIA0_PinGetPIN_PB()&0x08 ? "1" : "0",
		VIA0_PinGetPIN_PB()&0x04 ? "1" : "0",
		VIA0_PinGetPIN_PB()&0x02 ? "1" : "0",
		VIA0_PinGetPIN_PB()&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,6,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		VIA0_DDRB&0x80 ? "1" : "0",
		VIA0_DDRB&0x40 ? "1" : "0",
		VIA0_DDRB&0x20 ? "1" : "0",
		VIA0_DDRB&0x10 ? "1" : "0",
		VIA0_DDRB&0x08 ? "1" : "0",
		VIA0_DDRB&0x04 ? "1" : "0",
		VIA0_DDRB&0x02 ? "1" : "0",
		VIA0_DDRB&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,7,"-----------------VIA 2(9120) (vic20)");
	PrintAt(buffer,width,255,255,255,0,8,"PA = RW7 IN  RW6 IN  RW5 IN  RW4 IN  RW3 IN  RW2 IN  RW1 IN  RW0 IN ");
	PrintAt(buffer,width,255,255,255,0,9,"     %s       %s       %s       %s        %s       %s       %s       %s",
		VIA1_PinGetPIN_PA()&0x80 ? "1" : "0",
		VIA1_PinGetPIN_PA()&0x40 ? "1" : "0",
		VIA1_PinGetPIN_PA()&0x20 ? "1" : "0",
		VIA1_PinGetPIN_PA()&0x10 ? "1" : "0",
		VIA1_PinGetPIN_PA()&0x08 ? "1" : "0",
		VIA1_PinGetPIN_PA()&0x04 ? "1" : "0",
		VIA1_PinGetPIN_PA()&0x02 ? "1" : "0",
		VIA1_PinGetPIN_PA()&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,10,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		VIA1_DDRA&0x80 ? "1" : "0",
		VIA1_DDRA&0x40 ? "1" : "0",
		VIA1_DDRA&0x20 ? "1" : "0",
		VIA1_DDRA&0x10 ? "1" : "0",
		VIA1_DDRA&0x08 ? "1" : "0",
		VIA1_DDRA&0x04 ? "1" : "0",
		VIA1_DDRA&0x02 ? "1" : "0",
		VIA1_DDRA&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,11,"PB = CL7 OT  CL6 OT  CL5 OT  CL4 OT  CL3 OT  CL2 OT  CL1 OT  CL0 OT");
	PrintAt(buffer,width,255,255,255,0,12,"     %s       %s       %s       %s        %s       %s       %s       %s",
		VIA1_PinGetPIN_PB()&0x80 ? "1" : "0",
		VIA1_PinGetPIN_PB()&0x40 ? "1" : "0",
		VIA1_PinGetPIN_PB()&0x20 ? "1" : "0",
		VIA1_PinGetPIN_PB()&0x10 ? "1" : "0",
		VIA1_PinGetPIN_PB()&0x08 ? "1" : "0",
		VIA1_PinGetPIN_PB()&0x04 ? "1" : "0",
		VIA1_PinGetPIN_PB()&0x02 ? "1" : "0",
		VIA1_PinGetPIN_PB()&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,13,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		VIA1_DDRB&0x80 ? "1" : "0",
		VIA1_DDRB&0x40 ? "1" : "0",
		VIA1_DDRB&0x20 ? "1" : "0",
		VIA1_DDRB&0x10 ? "1" : "0",
		VIA1_DDRB&0x08 ? "1" : "0",
		VIA1_DDRB&0x04 ? "1" : "0",
		VIA1_DDRB&0x02 ? "1" : "0",
		VIA1_DDRB&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,14,"-----------------VIA 1(1800) (disk)");
//	PrintAt(buffer,width,255,255,255,0,1,"PA = ATN OT  CSW IN  PEN IN  JY2 IN  JY1 IN  JY0 IN  DAT IN  CLK IN ");
//	PrintAt(buffer,width,255,255,255,0,2,"     %s       %s       %s       %s        %s       %s       %s       %s",
//		VIA0_PinGetPIN_PA()&0x80 ? "1" : "0",
//		VIA0_PinGetPIN_PA()&0x40 ? "1" : "0",
//		VIA0_PinGetPIN_PA()&0x20 ? "1" : "0",
//		VIA0_PinGetPIN_PA()&0x10 ? "1" : "0",
//		VIA0_PinGetPIN_PA()&0x08 ? "1" : "0",
//		VIA0_PinGetPIN_PA()&0x04 ? "1" : "0",
//		VIA0_PinGetPIN_PA()&0x02 ? "1" : "0",
//		VIA0_PinGetPIN_PA()&0x01 ? "1" : "0");
//	PrintAt(buffer,width,255,255,255,0,3,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
//		VIA0_DDRA&0x80 ? "1" : "0",
//		VIA0_DDRA&0x40 ? "1" : "0",
//		VIA0_DDRA&0x20 ? "1" : "0",
//		VIA0_DDRA&0x10 ? "1" : "0",
//		VIA0_DDRA&0x08 ? "1" : "0",
//		VIA0_DDRA&0x04 ? "1" : "0",
//		VIA0_DDRA&0x02 ? "1" : "0",
//		VIA0_DDRA&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,15,"PB = DAT IN  DAT OT  CLK IN  CLK OT  ATN OT  DA0 IN  DA1 IN  ATN IN    "
		);
	PrintAt(buffer,width,255,255,255,0,16,"     %s       %s       %s       %s        %s       %s       %s       %s       CB2  ATN IN = %s",
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
	PrintAt(buffer,width,255,255,255,0,17,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA0_DDRB&0x80 ? "1" : "0",
		DISK_VIA0_DDRB&0x40 ? "1" : "0",
		DISK_VIA0_DDRB&0x20 ? "1" : "0",
		DISK_VIA0_DDRB&0x10 ? "1" : "0",
		DISK_VIA0_DDRB&0x08 ? "1" : "0",
		DISK_VIA0_DDRB&0x04 ? "1" : "0",
		DISK_VIA0_DDRB&0x02 ? "1" : "0",
		DISK_VIA0_DDRB&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,18,"-----------------VIA 2(1C00) (disk)");
	PrintAt(buffer,width,255,255,255,0,19,"PA = PA7     PA6     PA5     PA4     PA3     PA2     PA1     PA0    ");
	PrintAt(buffer,width,255,255,255,0,20,"     %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA1_PinGetPIN_PA()&0x80 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x40 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x20 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x10 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x08 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x04 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x02 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PA()&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,21,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA1_DDRA&0x80 ? "1" : "0",
		DISK_VIA1_DDRA&0x40 ? "1" : "0",
		DISK_VIA1_DDRA&0x20 ? "1" : "0",
		DISK_VIA1_DDRA&0x10 ? "1" : "0",
		DISK_VIA1_DDRA&0x08 ? "1" : "0",
		DISK_VIA1_DDRA&0x04 ? "1" : "0",
		DISK_VIA1_DDRA&0x02 ? "1" : "0",
		DISK_VIA1_DDRA&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,22,"PB = !SY IN  CK1 OT  CK0 OT  !WP IN  LED OT  ON  OT  SL1 OT  SL0 OT");
	PrintAt(buffer,width,255,255,255,0,23,"     %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA1_PinGetPIN_PB()&0x80 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x40 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x20 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x10 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x08 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x04 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x02 ? "1" : "0",
		DISK_VIA1_PinGetPIN_PB()&0x01 ? "1" : "0");
	PrintAt(buffer,width,255,255,255,0,24,"DDR  %s       %s       %s       %s        %s       %s       %s       %s",
		DISK_VIA1_DDRB&0x80 ? "1" : "0",
		DISK_VIA1_DDRB&0x40 ? "1" : "0",
		DISK_VIA1_DDRB&0x20 ? "1" : "0",
		DISK_VIA1_DDRB&0x10 ? "1" : "0",
		DISK_VIA1_DDRB&0x08 ? "1" : "0",
		DISK_VIA1_DDRB&0x04 ? "1" : "0",
		DISK_VIA1_DDRB&0x02 ? "1" : "0",
		DISK_VIA1_DDRB&0x01 ? "1" : "0");


}

