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
void DISK_PinSetPIN_SO(uint8_t);
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
extern int stopTheClock;

void DISK_Reset()
{
	DISK_PinSetPIN_SO(1);
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

int hackTime=11;

uint16_t DISK_Tick(uint8_t* clk,uint8_t* atn,uint8_t* dat)
{
	static uint16_t lastPC;
	uint16_t addr; 
	uint8_t pb=DISK_VIA0_PinGetPIN_PB()&0x9F;			// Clear drive number pins
	
	pb&=0x7A;
	pb|=(*clk)<<2;
	pb|=(*atn)<<7;
	pb|=*dat;
//	if ( ((pb&0x10)>>4) ^ (*atn) )
//		pb|=1;

	if (lastPA1!=pb)
	{
		printf("%d %d\n",(pb&0x10)>>4,*atn);
		lastPA1=pb;
	}

	if (lastClk!=*clk)
	{
		lastClk=*clk;
	}

	DISK_VIA0_PinSetPIN_PB(pb);
	DISK_VIA0_PinSetPIN_CA1(*atn);
	
	// We don't yet have a drive mechanism.. 
	//
	// CA2 pin is placed into a 3 input NAND gate.. it acts as enable for byte sync
	// for now.. if CA2 goes low, we wait 256 cycles and then act as if the drive sync occured
	
	if (DISK_VIA1_PinGetPIN_CA2()==1)
	{
		if (hackTime==0)
		{
			hackTime=0x100;
		}
		else
		{
			hackTime--;
		}
	}
	else
	{
		hackTime=0x100;
	}

	DISK_VIA1_PinSetPIN_CA1(hackTime==0?0:1);
	DISK_PinSetPIN_SO(hackTime==0?0:1);

	// DISK/CPU UPDATE
	DISK_PinSetPIN_O0(1);

	addr=DISK_PinGetPIN_AB();

	if (DISK_PinGetPIN_SYNC())
	{
		lastPC=addr;

		if (isBreakpoint(1,lastPC))
		{
			stopTheClock=1;
		}

		if (doDebug)
		{
			DISK_Disassemble(addr,1);
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

	return lastPC;
}

