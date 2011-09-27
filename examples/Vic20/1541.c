/*
 * 1540  (going off 1540 schematics + 1540 roms) drive implementation
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

uint16_t DISK_PinGetPIN_AB();
uint8_t DISK_PinGetPIN_DB();
void DISK_PinSetPIN_DB(uint8_t);
void DISK_PinSetPIN_O0(uint8_t);
uint8_t DISK_PinGetPIN_SYNC();
uint8_t DISK_PinGetPIN_RW();
void DISK_PinSetPIN__IRQ(uint8_t);
void DISK_PinSetPIN__RES(uint8_t);

void	DISK_VIA0_PinSetPIN_PA(uint8_t);
uint8_t DISK_VIA0_PinGetPIN_PA();
void	DISK_VIA0_PinSetPIN_CA1(uint8_t);
void	DISK_VIA0_PinSetPIN_CA2(uint8_t);
uint8_t DISK_VIA0_PinGetPIN_CA2();
void	DISK_VIA0_PinSetPIN_PB(uint8_t);
uint8_t DISK_VIA0_PinGetPIN_PB();
void	DISK_VIA0_PinSetPIN_CB1(uint8_t);
uint8_t DISK_VIA0_PinGetPIN_CB1();
void	DISK_VIA0_PinSetPIN_CB2(uint8_t);
uint8_t DISK_VIA0_PinGetPIN_CB2();
void	DISK_VIA0_PinSetPIN_RS(uint8_t);
void	DISK_VIA0_PinSetPIN_CS(uint8_t);
void	DISK_VIA0_PinSetPIN_D(uint8_t);
uint8_t DISK_VIA0_PinGetPIN_D();
void	DISK_VIA0_PinSetPIN__RES(uint8_t);
void	DISK_VIA0_PinSetPIN_O2(uint8_t);
void	DISK_VIA0_PinSetPIN_RW(uint8_t);
uint8_t DISK_VIA0_PinGetPIN__IRQ();

void	DISK_VIA1_PinSetPIN_PA(uint8_t);
uint8_t DISK_VIA1_PinGetPIN_PA();
void	DISK_VIA1_PinSetPIN_CA1(uint8_t);
void	DISK_VIA1_PinSetPIN_CA2(uint8_t);
uint8_t DISK_VIA1_PinGetPIN_CA2();
void	DISK_VIA1_PinSetPIN_PB(uint8_t);
uint8_t DISK_VIA1_PinGetPIN_PB();
void	DISK_VIA1_PinSetPIN_CB1(uint8_t);
uint8_t DISK_VIA1_PinGetPIN_CB1();
void	DISK_VIA1_PinSetPIN_CB2(uint8_t);
uint8_t DISK_VIA1_PinGetPIN_CB2();
void	DISK_VIA1_PinSetPIN_RS(uint8_t);
void	DISK_VIA1_PinSetPIN_CS(uint8_t);
void	DISK_VIA1_PinSetPIN_D(uint8_t);
uint8_t DISK_VIA1_PinGetPIN_D();
void	DISK_VIA1_PinSetPIN__RES(uint8_t);
void	DISK_VIA1_PinSetPIN_O2(uint8_t);
void	DISK_VIA1_PinSetPIN_RW(uint8_t);
uint8_t DISK_VIA1_PinGetPIN__IRQ();

void DISK_VIADumpInfo(int chipNo)
{
#if 0
	printf("--VIA--%d\n",chipNo+1);
	if (chipNo==0)
	{
		printf("PB %02X/%02X %s:%s:%s:%s:%s:%s:%s:%s  CA1/CA2 %02X/%02X\n",DISK_via[chipNo].IRB,DISK_via[chipNo].ORB,
				"ATN__I",
				"DEV_AD",
				"DEV_AD",
				"ATNA_O",
				"CLCK_O",
				"CLCK_I",
				"DATA_O",
				"DATA_I",
				DISK_via[chipNo].CA1,DISK_via[chipNo].CA2);
		printf("PA %02X/%02X %s:%s:%s:%s:%s:%s:%s:%s  CB1/CB2 %02X/%02X\n",DISK_via[chipNo].IRB,DISK_via[chipNo].ORB,
				"      ",
				"      ",
				"      ",
				"      ",
				"      ",
				"      ",
				"      ",
				"      ",
				DISK_via[chipNo].CB1,DISK_via[chipNo].CB2);
	}
	else
	{
		printf("PB %02X/%02X %s:%s:%s:%s:%s:%s:%s:%s  CA1/CA2 %02X/%02X\n",DISK_via[chipNo].IRB,DISK_via[chipNo].ORB,
				"SYNC_I",
				"BRAT_?",
				"BRAT_?",
				"WPRT_O",
				"LED__O",
				"MTR__O",
				"STPM_O",
				"STPM_O",
				DISK_via[chipNo].CA1,DISK_via[chipNo].CA2);
		printf("PA %02X/%02X %s:%s:%s:%s:%s:%s:%s:%s  CB1/CB2 %02X/%02X\n",DISK_via[chipNo].IRB,DISK_via[chipNo].ORB,
				"      ",
				"      ",
				"      ",
				"      ",
				"      ",
				"      ",
				"      ",
				"      ",
				DISK_via[chipNo].CB1,DISK_via[chipNo].CB2);
	}
	printf("DDRB/DDRA = %02X/%02X\n",DISK_via[chipNo].DDRB,DISK_via[chipNo].DDRA);
	printf("IFR/IER/ACR/PCR/T1C/T2C = %02X/%02X/%02X/%02X/%04X/%04X\n",DISK_via[chipNo].IFR,DISK_via[chipNo].IER,DISK_via[chipNo].ACR,DISK_via[chipNo].PCR,DISK_via[chipNo].T1C,DISK_via[chipNo].T2C);
	printf("--------\n");
#endif
}


// Step 1. Memory

unsigned char DiskRam[0x0800];
unsigned char DiskRomLo[0x2000];
unsigned char DiskRomHi[0x2000];

int LoadRom(unsigned char* rom,unsigned int size,const char* fname);

void DISK_VIADumpInfo(int chipNo);

int DISK_InitialiseMemory()
{
	if (LoadRom(DiskRomLo,0x2000,"roms/1540-c000.325302-01.bin"))
		return 1;
	if (LoadRom(DiskRomHi,0x2000,"roms/1540-e000.325303-01.bin"))
		return 1;

	return 0;
}

uint8_t DISK_VIAGetByte(int chipNo,int regNo);
void DISK_VIASetByte(int chipNo,int regNo,uint8_t byte);
void DISK_VIATick(int chipNo);

uint8_t DISK_GetByte(uint16_t addr)
{
	if (addr<0x0800)
	{
		return DiskRam[addr];
	}
	if (addr<0x1800)
	{
		return 0xFF;
	}
	if (addr<0x1810)
	{
		return DISK_VIA0_PinGetPIN_D();
	}
	if (addr<0x1C00)
	{
		return 0xFF;
	}
	if (addr<0x1C10)
	{
		return DISK_VIA1_PinGetPIN_D();
	}
	if (addr<0xC000)
	{
		return 0xFF;
	}
	if (addr<0xE000)
	{
		return DiskRomLo[addr-0xC000];
	}
	return DiskRomHi[addr-0xE000];
}

void DISK_SetByte(uint16_t addr,uint8_t byte)
{
	if (addr<0x0800)
	{
		DiskRam[addr]=byte;
		return;
	}
	if (addr<0x1800)
	{
		return;
	}
	if (addr<0x1810)
	{
		DISK_VIA0_PinSetPIN_D(byte);
		return;
	}
	if (addr<0x1C00)
	{
		return;
	}
	if (addr<0x1C10)
	{
		DISK_VIA1_PinSetPIN_D(byte);
		return;
	}
	return;
}


int diskClock=0;

extern uint8_t *DISK_DIS_[256];

extern uint8_t	DISK_A;
extern uint8_t	DISK_X;
extern uint8_t	DISK_Y;
extern uint16_t	DISK_SP;
extern uint16_t	DISK_PC;
extern uint8_t	DISK_P;
#if 0
struct DISK_via6522
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

struct DISK_via6522	DISK_via[2];
#endif
void DISK_DUMP_REGISTERS()
{
	printf("--------\n");
	printf("FLAGS = N  V  -  B  D  I  Z  C\n");
	printf("        %s  %s  %s  %s  %s  %s  %s  %s\n",
			DISK_P&0x80 ? "1" : "0",
			DISK_P&0x40 ? "1" : "0",
			DISK_P&0x20 ? "1" : "0",
			DISK_P&0x10 ? "1" : "0",
			DISK_P&0x08 ? "1" : "0",
			DISK_P&0x04 ? "1" : "0",
			DISK_P&0x02 ? "1" : "0",
			DISK_P&0x01 ? "1" : "0");
	printf("A = %02X\n",DISK_A);
	printf("X = %02X\n",DISK_X);
	printf("Y = %02X\n",DISK_Y);
	printf("SP= %04X\n",DISK_SP);
	printf("--------\n");
	DISK_VIADumpInfo(0);
	DISK_VIADumpInfo(1);
}

const char* DISK_decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

	uint8_t byte = DISK_GetByte(address);
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
				sprintf(sprintBuffer,"%02X",DISK_GetByte(address+offset));
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

int DISK_Disassemble(unsigned int address,int registers)
{
	int a;
	int numBytes=0;
	const char* retVal = DISK_decodeDisasm(DISK_DIS_,address,&numBytes,255);

	if (strcmp(retVal,"UNKNOWN OPCODE")==0)
	{
		printf("UNKNOWN AT : %04X\n",address);
		for (a=0;a<numBytes+1;a++)
		{
			printf("%02X ",DISK_GetByte(address+a));
		}
		printf("\n");
		exit(-1);
	}

	if (registers)
	{
		DISK_DUMP_REGISTERS();
	}
	printf("%04X :",address);

	for (a=0;a<numBytes+1;a++)
	{
		printf("%02X ",DISK_GetByte(address+a));
	}
	for (a=0;a<8-(numBytes+1);a++)
	{
		printf("   ");
	}
	printf("%s\n",retVal);

	return numBytes+1;
}

extern int doDebug;

void DISK_Reset()
{
	DISK_PinSetPIN__IRQ(1);

	DISK_PC=DISK_GetByte(0xFFFC);
	DISK_PC|=DISK_GetByte(0xFFFD)<<8;
	
	DISK_VIA0_PinSetPIN__RES(0);
	DISK_VIA0_PinSetPIN_O2(0);
	DISK_VIA0_PinSetPIN_O2(1);
	DISK_VIA0_PinSetPIN_O2(0);
	DISK_VIA0_PinSetPIN_O2(1);
	DISK_VIA0_PinSetPIN_O2(0);
	DISK_VIA0_PinSetPIN__RES(1);

	DISK_VIA1_PinSetPIN__RES(0);
	DISK_VIA1_PinSetPIN_O2(0);
	DISK_VIA1_PinSetPIN_O2(1);
	DISK_VIA1_PinSetPIN_O2(0);
	DISK_VIA1_PinSetPIN_O2(1);
	DISK_VIA1_PinSetPIN_O2(0);
	DISK_VIA1_PinSetPIN__RES(1);
}

uint8_t lastPA1=0xFF;
uint8_t lastDrvPB=0;
static uint8_t lastClk=12;
void DISK_Tick(uint8_t* clk,uint8_t* atn,uint8_t* dat)
{
	uint16_t addr; 
	uint8_t pb=DISK_VIA0_PinGetPIN_PB()&0x9F;			// Clear drive number pins
	
	pb&=0x7A;
	pb|=(*clk)<<2;
	pb|=(*atn)<<7;
	pb|=*dat;
	if ( ((pb&0x10)>>4) ^ (*atn) )
		pb|=1;

	if (lastPA1!=pb)
	{
		printf("%d %d\n",(pb&0x10)>>4,*atn);
		lastPA1=pb;
	}

	if (lastClk!=*clk)
	{
		lastClk=*clk;
//		doDebug=1;
	}

	DISK_VIA0_PinSetPIN_PB(pb);
	DISK_VIA0_PinSetPIN_CA1(*atn);

	DISK_VIA1_PinSetPIN_CA1(1);		//~BYTE SYNC

	// DISK/CPU UPDATE
	DISK_PinSetPIN_O0(1);

	addr=DISK_PinGetPIN_AB();

	if (DISK_PinGetPIN_SYNC())
	{
//		if (DISK_PinGetPIN_AB()==0xEBE7)		//F2B0 - irq
//			doDebug=1;
		if (DISK_PinGetPIN_AB()==0xE85B)		//ATN Ack
			doDebug=1;
		if (DISK_PinGetPIN_AB()==0xE9F0)		//CLOCK CHANGED
			doDebug=1;
		if (DISK_PinGetPIN_AB()==0xEA6B)		//EOI
			doDebug=1;
//		if (DISK_PinGetPIN_AB()==0xF2B0)		//Timer interrupted
//			doDebug=1;
//		if (DISK_PinGetPIN_AB()==0xE853)		//ATN Interrupt
//			doDebug=1;
//		if (DISK_PinGetPIN_AB()==0xEB3A)		//Startup
//			doDebug=1;
//		if (DISK_PinGetPIN_AB()==0xE884)		//atn seen
//			doDebug=1;


		if (doDebug)
		{
			DISK_Disassemble(addr,1);
			if (getch()==' ')
			{
				doDebug=0;
			}
		}
	}


	DISK_VIA0_PinSetPIN_RW(DISK_PinGetPIN_RW());
	DISK_VIA1_PinSetPIN_RW(DISK_PinGetPIN_RW());

	DISK_VIA0_PinSetPIN_RS(addr&0xF);
	DISK_VIA1_PinSetPIN_RS(addr&0xF);

	if ((addr&0x9C00)==0x1800)		// VIA0
	{
		if (doDebug)
			printf("%04X : CS SETTING\n",addr);
		DISK_VIA0_PinSetPIN_CS(0x01|0x00);
		DISK_VIA1_PinSetPIN_CS(0x01|0x02);
	}
	else
	{
		if ((addr&0x9C00)==0x1C00)	// VIA1
		{
			if (doDebug)
				printf("%04X : CS SETTING\n",addr);
			DISK_VIA0_PinSetPIN_CS(0x01|0x02);
			DISK_VIA1_PinSetPIN_CS(0x01|0x00);
		}
		else
		{
			if (doDebug)
				printf("%04X : CS CLEARING\n",addr);
			DISK_VIA0_PinSetPIN_CS(0x01|0x02);
			DISK_VIA1_PinSetPIN_CS(0x01|0x02);
		}
	}

	DISK_VIA0_PinSetPIN_O2(1);
	DISK_VIA1_PinSetPIN_O2(1);

	if (DISK_PinGetPIN_RW())
	{
		uint8_t  data = DISK_GetByte(addr);
//		if (doDebug)
//			printf("Getting : %02X @ %04X\n", data,DISK_PinGetPIN_AB());
		DISK_PinSetPIN_DB(data);
	}
	if (!DISK_PinGetPIN_RW())
	{
//		if (doDebug)
//			printf("Storing : %02X @ %04X\n", DISK_PinGetPIN_DB(),DISK_PinGetPIN_AB());
		DISK_SetByte(addr,DISK_PinGetPIN_DB());
	}

	DISK_VIA0_PinSetPIN_O2(0);
	DISK_VIA1_PinSetPIN_O2(0);
		
	DISK_PinSetPIN__IRQ(DISK_VIA0_PinGetPIN__IRQ()&DISK_VIA1_PinGetPIN__IRQ());

	DISK_PinSetPIN_O0(0);

	// END CPU/DISK UPDATE

	if (lastDrvPB!=DISK_VIA1_PinGetPIN_PB())
	{
		lastDrvPB=DISK_VIA1_PinGetPIN_PB();
		printf("Drive change %02X\n",lastDrvPB);
		printf("Stepping %d\n",lastDrvPB&0x03);
		printf("LED %s\n",lastDrvPB&0x08?"on":"off");
		printf("Motor %s\n",lastDrvPB&0x04?"on":"off");
		printf("WriteProt %s\n",lastDrvPB&0x10?"on":"off");
		printf("BitRate %d\n",(lastDrvPB&0x60)>>5);
		printf("Sync %d\n",(lastDrvPB&0x80)>>7);
	}

	*clk=(DISK_VIA0_PinGetPIN_PB()&0x08)>>3;
	*dat=(DISK_VIA0_PinGetPIN_PB()&0x02)>>1;
	*atn=(DISK_VIA0_PinGetPIN_PB()&0x10)>>4;
#if 0
	DISK_via[0].IRB=0xFA;
	DISK_via[0].IRB|=CA2<<2;
	DISK_via[0].IRB|=CB2;

	if ((lastPA1&0x80)==0 && (PA1&0x80)==0x80)
	{
		if ((DISK_via[0].PCR&0xE0)==0)
		{
			DISK_via[0].IFR|=0x02;
		}
	}
	DISK_VIATick(0);
	DISK_VIATick(1);


#endif
//	lastPA1=PA1;
//	return DISK_VIA0_PinGetPIN_PB();









#if 0

	static uint8_t lastCA2=0xFF,lastCB2=0xFF,lastDrvPB=0xFF,lastIOPB=0xFF;
	uint16_t addr; 

	if (lastIOPB!=DISK_VIA0_PinGetPIN_PB())
	{
		lastIOPB=DISK_VIA0_PinGetPIN_PB();
		printf("IO change %02X\n",lastIOPB);
	}

	if (lastCA2!=CA2)
	{
		printf("DISK Data Change %d\n",CA2);
	}
	if (lastCB2!=CB2)
	{
		printf("DISK Clock Change %d\n",CB2);
	}
/*
	if ((lastPA1&0x83)!=(PA1&0x83))
	{
		printf("DISK Par Change %02X\n",PA1);
	}*/
	lastCB2=CB2;
	lastCA2=CA2;

	DISK_PinSetPIN_O0(1);

	addr=DISK_PinGetPIN_AB();

	if (DISK_PinGetPIN_SYNC())
	{
//		if (DISK_PinGetPIN_AB()==0xEBE7)		//F2B0 - irq
//			doDebug=1;
//		if (DISK_PinGetPIN_AB()==0xE85B)		//ATN Ack
//			doDebug=1;
//		if (DISK_PinGetPIN_AB()==0xF2B0)		//Timer interrupted
//			doDebug=1;
//		if (DISK_PinGetPIN_AB()==0xE853)		//ATN Interrupt
//			doDebug=1;
//		if (DISK_PinGetPIN_AB()==0xEB3A)		//Startup
//			doDebug=1;


		if (doDebug)
		{
			DISK_Disassemble(addr,1);
			getch();
		}
	}
//	PB0 - DATA IN				PB0	STEP MOTOR
//	PB1 - DATA OUT				PB1	STEP MOTOR
//	PB2 - CLOCK IN				PB2	MTR / drive motor (on/off)
//	PB3 - CLOCK OUT				PB3	ACT / LED on drive
//	PB4 - ATN ACK OUT			PB4	Write Protect Sense (1==on)
//	PB5 - DEVICE ADDRESS			PB5	BIT RATE
//	PB6 - DEVICE ADDRESS			PB6	BIT RATE
//	PB7 - ATN IN				PB7	SYNC IN

	if (lastDrvPB!=DISK_VIA1_PinGetPIN_PB())
	{
		lastDrvPB=DISK_VIA1_PinGetPIN_PB();
		printf("Drive change %02X\n",lastDrvPB);
		printf("Stepping %d\n",lastDrvPB&0x03);
		printf("LED %s\n",lastDrvPB&0x08?"on":"off");
		printf("Motor %s\n",lastDrvPB&0x04?"on":"off");
		printf("WriteProt %s\n",lastDrvPB&0x10?"on":"off");
		printf("BitRate %d\n",(lastDrvPB&0x60)>>5);
		printf("Sync %d\n",(lastDrvPB&0x80)>>7);
	}
	

	DISK_VIA0_PinSetPIN_RW(DISK_PinGetPIN_RW());
	DISK_VIA1_PinSetPIN_RW(DISK_PinGetPIN_RW());

	DISK_VIA0_PinSetPIN_RS(addr&0xF);
	DISK_VIA1_PinSetPIN_RS(addr&0xF);

	if ((addr&0x9C00)==0x1800)		// VIA0
	{
		if (doDebug)
			printf("%04X : CS SETTING\n",addr);
		DISK_VIA0_PinSetPIN_CS(0x01|0x00);
		DISK_VIA1_PinSetPIN_CS(0x01|0x02);
	}
	else
	{
		if ((addr&0x9C00)==0x1C00)	// VIA1
		{
			if (doDebug)
				printf("%04X : CS SETTING\n",addr);
			DISK_VIA0_PinSetPIN_CS(0x01|0x02);
			DISK_VIA1_PinSetPIN_CS(0x01|0x00);
		}
		else
		{
			if (doDebug)
				printf("%04X : CS CLEARING\n",addr);
			DISK_VIA0_PinSetPIN_CS(0x01|0x02);
			DISK_VIA1_PinSetPIN_CS(0x01|0x02);
		}
	}

	DISK_VIA0_PinSetPIN_O2(1);
	DISK_VIA1_PinSetPIN_O2(1);

	if (DISK_PinGetPIN_RW())
	{
		uint8_t  data = DISK_GetByte(addr);
//		if (doDebug)
//			printf("Getting : %02X @ %04X\n", data,DISK_PinGetPIN_AB());
		DISK_PinSetPIN_DB(data);
	}
	if (!DISK_PinGetPIN_RW())
	{
//		if (doDebug)
//			printf("Storing : %02X @ %04X\n", DISK_PinGetPIN_DB(),DISK_PinGetPIN_AB());
		DISK_SetByte(addr,DISK_PinGetPIN_DB());
	}

	DISK_VIA0_PinSetPIN_O2(0);
	DISK_VIA1_PinSetPIN_O2(0);
		
	DISK_PinSetPIN__IRQ(DISK_VIA0_PinGetPIN__IRQ()&DISK_VIA1_PinGetPIN__IRQ());

	DISK_PinSetPIN_O0(0);
#if 0
	DISK_via[0].IRB=0xFA;
	DISK_via[0].IRB|=CA2<<2;
	DISK_via[0].IRB|=CB2;

	if ((lastPA1&0x80)==0 && (PA1&0x80)==0x80)
	{
		if ((DISK_via[0].PCR&0xE0)==0)
		{
			DISK_via[0].IFR|=0x02;
		}
	}
	DISK_VIATick(0);
	DISK_VIATick(1);


#endif
	lastPA1=PA1;
	return DISK_VIA0_PinGetPIN_PB();


#endif
}

#if 0

///////////////////
//

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


uint8_t DISK_VIAGetByte(int chipNo,int regNo)
{
	if (doDebug)
		printf("R VIA%d %02X\n",chipNo+1,regNo);
	switch (regNo)
	{
		case 0:			//IRB
			DISK_via[chipNo].IFR&=~0x18;
			return (DISK_via[chipNo].IRB & (~DISK_via[chipNo].DDRB)) | (DISK_via[chipNo].ORB & DISK_via[chipNo].DDRB);
		case 1:			//IRA
			DISK_via[chipNo].IFR&=~0x03;
			//FALL through intended
		case 15:
			return (DISK_via[chipNo].IRA & (~DISK_via[chipNo].DDRA));
		case 2:
			return DISK_via[chipNo].DDRB;
		case 3:
			return DISK_via[chipNo].DDRA;
		case 4:
			DISK_via[chipNo].IFR&=~0x40;
			return (DISK_via[chipNo].T1C&0xFF);
		case 5:
			return (DISK_via[chipNo].T1C>>8);
		case 6:
			return DISK_via[chipNo].T1LL;
		case 7:
			return DISK_via[chipNo].T1LH;
		case 8:
			DISK_via[chipNo].IFR&=~0x20;
			return (DISK_via[chipNo].T2C&0xFF);
		case 9:
			return (DISK_via[chipNo].T2C>>8);
		case 10:
			DISK_via[chipNo].IFR&=~0x04;
			return DISK_via[chipNo].SR;
		case 11:
			return DISK_via[chipNo].ACR;
		case 12:
			return DISK_via[chipNo].PCR;
		case 13:
			return DISK_via[chipNo].IFR;
		case 14:
			return DISK_via[chipNo].IER & 0x7F;
	}
}

void DISK_VIASetByte(int chipNo,int regNo,uint8_t byte)
{
	if (doDebug)
		printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
	switch (regNo)
	{
		case 0:
			DISK_via[chipNo].IFR&=~0x18;
			DISK_via[chipNo].ORB=byte&DISK_via[chipNo].DDRB;
			break;
		case 1:
			DISK_via[chipNo].IFR&=~0x03;
			//FALL through intended
		case 15:
			DISK_via[chipNo].ORA=byte&DISK_via[chipNo].DDRA;
			break;
		case 2:
//			if (DISK_via[chipNo].DDRB!=byte)
//				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			DISK_via[chipNo].DDRB=byte;
			break;
		case 3:
//			if (DISK_via[chipNo].DDRA!=byte)
//				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			DISK_via[chipNo].DDRA=byte;
			break;
		case 4:
			DISK_via[chipNo].T1LL=byte;
			break;
		case 5:
			DISK_via[chipNo].T1LH=byte;
			DISK_via[chipNo].T1C=byte<<8;
			DISK_via[chipNo].T1C|=DISK_via[chipNo].T1LL;
			DISK_via[chipNo].IFR&=~0x40;
			break;
		case 6:
			DISK_via[chipNo].T1LL=byte;
			break;
		case 7:
			DISK_via[chipNo].T1LH=byte;
			DISK_via[chipNo].IFR&=~0x40;
			break;
		case 8:
			DISK_via[chipNo].T2LL=byte;
			break;
		case 9:
			DISK_via[chipNo].T2TimerOff=0;
			DISK_via[chipNo].T2C=byte<<8;
			DISK_via[chipNo].T2C|=DISK_via[chipNo].T2LL;
			DISK_via[chipNo].IFR&=~0x20;
			break;
		case 10:
			DISK_via[chipNo].IFR&=~0x04;
			DISK_via[chipNo].SR=byte;
			break;
		case 11:
//			if (DISK_via[chipNo].ACR!=byte)
//				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			DISK_via[chipNo].ACR=byte;
			break;
		case 12:
			if (DISK_via[chipNo].PCR!=byte  && chipNo==0)
				printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
			DISK_via[chipNo].PCR=byte;
			break;
		case 13:
			byte&=0x7F;
			byte=~byte;
			DISK_via[chipNo].IFR&=byte;
			break;
		case 14:
			if (byte&0x80)
			{
				DISK_via[chipNo].IER|=byte&0x7F;
			}
			else
			{
				DISK_via[chipNo].IER&=~(byte&0x7F);
			}
			break;
	}
}

void DISK_VIATick(int chipNo)
{
	DISK_via[chipNo].IRA=0x00;
	DISK_via[chipNo].IRB=0x00;

	if (DISK_via[chipNo].T1C)
	{
		DISK_via[chipNo].T1C--;
		if (DISK_via[chipNo].T1C==0)
		{
			DISK_via[chipNo].IFR|=0x40;
			if (DISK_via[chipNo].ACR&0x40)
			{
				DISK_via[chipNo].T1C=DISK_via[chipNo].T1LH<<8;
				DISK_via[chipNo].T1C|=DISK_via[chipNo].T1LL;
			}
//			printf("T1 Counter Underflow On VIA %d\n",chipNo);
		}
	}
	DISK_via[chipNo].T2C--;
	if ((DISK_via[chipNo].T2C==0) && (DISK_via[chipNo].T2TimerOff==0))
	{
		DISK_via[chipNo].IFR|=0x20;
		DISK_via[chipNo].T2TimerOff=1;
//		printf("T2 Counter Underflow On VIA %d\n",chipNo);
	}

	switch (DISK_via[chipNo].PCR&0x0E)
	{
		case 0x0:
			//Input Mode - 
			break;
		case 0x2:
		case 0x4:
		case 0x6:
		case 0x8:
		case 0xA:
			printf("Warning Unsupported PCR CA2 mode (%d)%d\n",chipNo+1,(DISK_via[chipNo].PCR&0xE)>>1);
			break;
		case 0xC:
			DISK_via[chipNo].CA2=0x0;
			break;
		case 0xE:
			DISK_via[chipNo].CA2=0x1;
			break;
	}
	switch (DISK_via[chipNo].PCR&0xE0)
	{
		case 0x00:
			//Input Mode -
			break;
		case 0x20:
		case 0x40:
		case 0x60:
		case 0x80:
		case 0xA0:
			printf("Warning Unsupported PCR CB2 mode (%d)%d\n",chipNo+1,(DISK_via[chipNo].PCR&0xE)>>1);
			break;
		case 0xC0:
			DISK_via[chipNo].CB2=0x0;
			break;
		case 0xE0:
			DISK_via[chipNo].CB2=0x1;
			break;
	}
#if 0
	if (chipNo==0)
	{
		uint8_t joyVal=0xFF;
		joyVal&=KeyDown(GLFW_KEY_KP_8)?0xFB:0xFF;
		joyVal&=KeyDown(GLFW_KEY_KP_2)?0xF7:0xFF;
		joyVal&=KeyDown(GLFW_KEY_KP_4)?0xEF:0xFF;
		joyVal&=KeyDown(GLFW_KEY_KP_0)?0xDF:0xFF;
		DISK_via[chipNo].IRA=joyVal;

#if 0
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
			DISK_via[chipNo].IRA&=0xBF;
		}
	}
	if (chipNo==1)
	{
		// TODO : some keys require that shift is held down - F2,F4,CURSOR LEFT etc
		uint8_t keyVal=0xFF;
		keyVal&=CheckKeys(0x01,'1','3','5','7','9','-','=',GLFW_KEY_BACKSPACE);
		keyVal&=CheckKeys(0x02,'`','W','R','Y','I','P',']',GLFW_KEY_ENTER);
		keyVal&=CheckKeys(0x04,GLFW_KEY_LCTRL,'A','D','G','J','L','\'',GLFW_KEY_RIGHT);
		keyVal&=CheckKeys(0x08,GLFW_KEY_TAB,GLFW_KEY_LSHIFT,'X','V','N',',','/',GLFW_KEY_DOWN);
		keyVal&=CheckKeys(0x10,GLFW_KEY_SPACE,'Z','C','B','M','.',GLFW_KEY_RSHIFT,GLFW_KEY_F1);
		keyVal&=CheckKeys(0x20,GLFW_KEY_RCTRL,'S','F','H','K',';','#',GLFW_KEY_F3);
		keyVal&=CheckKeys(0x40,'Q','E','T','U','O','[','/',GLFW_KEY_F5);
		keyVal&=CheckKeys(0x80,'2','4','6','8','0','\\',GLFW_KEY_HOME,GLFW_KEY_F7);
		DISK_via[chipNo].IRA=keyVal;
		
		uint8_t joyVal=0xFF;
		joyVal&=KeyDown(GLFW_KEY_KP_6)?0x7F:0xFF;
		DISK_via[chipNo].IRB=joyVal;
		
		if (playDown && recDown && (!DISK_via[0].CA2)) // SAVING
		{
			casCount++;
			if ((DISK_via[chipNo].ORB&0x08)!=casLevel)
			{
				cntBuffer[cntPos++]=casCount;
				casCount=0;
				casLevel=(DISK_via[chipNo].ORB&0x08);
			}
		}
		if (playDown && (!recDown) && (!DISK_via[0].CA2)) // LOADING
		{
			casCount++;
			if (casCount>=cntBuffer[cntPos])
			{
				if (casLevel==0 && (DISK_via[chipNo].PCR&0x01))
				{
					DISK_via[chipNo].IFR|=0x02;
				}
				if (casLevel==1 && ((DISK_via[chipNo].PCR&0x01)==0))
				{
					DISK_via[chipNo].IFR|=0x02;
				}
				casLevel^=1;
				cntPos++;
				casCount=0;
			}
		}
	}
#endif
	DISK_via[chipNo].IFR&=0x7F;
	if ((DISK_via[chipNo].IFR&0x7F)&(DISK_via[chipNo].IER&0x7F))
	{
		DISK_via[chipNo].IFR|=0x80;
		DISK_PinSetPIN__IRQ(0);
	}
	else
	{
		DISK_PinSetPIN__IRQ(1);
	}
}

#endif
