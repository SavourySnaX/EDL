/*
 * 1541 
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "gui\debugger.h"

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

// Step 1. Memory

unsigned char DiskRam[0x0800];
unsigned char DiskRomLo[0x2000];
unsigned char DiskRomHi[0x2000];

unsigned char combinedRom[0x4000];
//unsigned char DiskData[252004];
unsigned char DiskData[333744];

int LoadRom(unsigned char* rom,unsigned int size,const char* fname);

int curTrack=-1;
void MoveHead(int);

#define USE_DRW_IMAGE	1
int LoadD64(const char* filename);

int DISK_InitialiseMemory()
{
#if 0
	if (LoadRom(DiskRomLo,0x2000,"roms/1540-c000.325302-01.bin"))
		return 1;
	if (LoadRom(DiskRomHi,0x2000,"roms/1540-e000.325303-01.bin"))
		return 1;
#else
	if (LoadRom(combinedRom,0x4000,"roms/dos1541"))
		return 1;
	memcpy(DiskRomLo,combinedRom,0x2000);
	memcpy(DiskRomHi,combinedRom+0x2000,0x2000);
#endif
#if USE_DRW_IMAGE
	LoadD64("disks/aufmonty.d64");
	
//	if (LoadRom(DiskData,252004,"disks/vicdemos3b.drw"))
//		return 1;
#else
	if (LoadRom(DiskData,333744,"disks/party.g64"))
		return 1;
#endif

	if (curTrack<0)
	{
		MoveHead(1);			// Initialise DISK
	}

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

#define DISK_CALC(x)	(((16*4)<<2)/x)

int diskStepTime=DISK_CALC(13);

int hackTime=DISK_CALC(13);

const uint32_t trackStartTable[36]={
/*1 */0x00000000,
/*2 */0x00001E0C,
/*3 */0x00003C18,
/*4 */0x00005A24,
/*5 */0x00007830,
/*6 */0x0000963C,
/*7 */0x0000B448,
/*8 */0x0000D254,
/*9 */0x0000F060,
/*10*/0x00010E6C,
/*11*/0x00012C78,
/*12*/0x00014A84,
/*13*/0x00016890,
/*14*/0x0001869C,
/*15*/0x0001A4A8,
/*16*/0x0001C2B4,
/*17*/0x0001E0C0,
/*18*/0x0001FECC,
/*19*/0x00021AB2,
/*20*/0x00023698,
/*21*/0x0002527E,
/*22*/0x00026E64,
/*23*/0x00028A4A,
/*24*/0x0002A630,
/*25*/0x0002C216,
/*26*/0x0002DC20,
/*27*/0x0002F62A,
/*28*/0x00031034,
/*29*/0x00032A3E,
/*30*/0x00034448,
/*31*/0x00035E52,
/*32*/0x000376BC,
/*33*/0x00038F26,
/*34*/0x0003A790,
/*35*/0x0003BFFA,
      0x0003D864
};

uint32_t dataOffsetForTrack=0;//0x1fecc;//0x5fa;//trackStartTable[17];
uint32_t headPosition=0;			// Cur bit position
uint32_t headMaxForTrack=0x1e0c;//(0x21ab2-0x1fecc)<<3;//trackStartTable[18]-trackStartTable[17])<<3;		// Last bit position

uint8_t ReadBit()
{
	uint32_t headPosByte=headPosition>>3;
	uint32_t headPosBit=headPosition&7;

	return (DiskData[dataOffsetForTrack + headPosByte]&(1<<(7-headPosBit))) >> (7-headPosBit);
}

void WriteBit(uint8_t bit)
{
	uint32_t headPosByte=headPosition>>3;
	uint32_t headPosBit=headPosition&7;

	DiskData[dataOffsetForTrack + headPosByte]&=~(1<<(7-headPosBit));
	DiskData[dataOffsetForTrack + headPosByte]|=bit<<(7-headPosBit);
}

void RotateDisk()
{
	headPosition++;
	if (headPosition >= headMaxForTrack)
	{
		headPosition=0;
		printf("Track Wrap\n");
	}
}

#if USE_DRW_IMAGE
void MoveHead(int dir)
{
	curTrack+=dir;
	if (curTrack > (35<<1))
		curTrack=35<<1;
	if (curTrack < 0)
		curTrack=0;

	dataOffsetForTrack=trackStartTable[curTrack>>1];
	headMaxForTrack=(trackStartTable[(curTrack>>1)+1]-trackStartTable[curTrack>>1])<<3;
	
	diskStepTime=DISK_CALC(16);
	if ((curTrack>>1)<30)
	{
		diskStepTime=DISK_CALC(15);
	}
	if ((curTrack>>1)<24)
	{
		diskStepTime=DISK_CALC(14);
	}
	if ((curTrack>>1)<17)
	{
		diskStepTime=DISK_CALC(13);
	}

	printf("Track Position : %08X, Track Length : %04X\n",dataOffsetForTrack,headMaxForTrack>>3);
	
	if (headPosition >= headMaxForTrack)
	{
		printf("Track Wrap\n");
		headPosition=0;
	}
}
#else
void MoveHead(int dir)
{
	curTrack+=dir;
	if (curTrack > (35<<1))
		curTrack=35<<1;
	if (curTrack < 0)
		curTrack=0;

	{
		uint32_t trackPosition;
		uint16_t trackLength;

		trackPosition =DiskData[0x0C + (curTrack>>1)*8 + 0];
		trackPosition|=DiskData[0x0C + (curTrack>>1)*8 + 1]<<8;
		trackPosition|=DiskData[0x0C + (curTrack>>1)*8 + 2]<<16;
		trackPosition|=DiskData[0x0C + (curTrack>>1)*8 + 3]<<24;

		trackLength = DiskData[trackPosition+0];
		trackLength|= DiskData[trackPosition+1]<<8;

		printf("Track Position : %08X, Track Length : %04X\n",trackPosition,trackLength);

		dataOffsetForTrack=trackPosition+2;
		headMaxForTrack=trackLength<<3;
		if (headPosition >= headMaxForTrack)
		{
			printf("Track Wrap\n");
			headPosition=0;
		}
	}
}
#endif

// Not sure about this yet, in principal the hardware needs someway to align itself to the bit stream.. the SYNC data is a stream of 1 bits, followed by at least 1 zero (in standard dos tracks anyway)
//Best guess, if all bits in pa port are 1, set sync flag - keep stepping 1 bit until, as soon as pa bits are not all 1, the next 8 bits (including the 0 will form the first valid byte).

uint16_t pBuffer=0;
uint8_t bufBits=0;
int byteAvailable=0;
int syncMark=0;
int lastBit=0;

int lastStep=0;

void HandleDiskControllerHardware()		// Every 13(14,15,16) clocks.. advance 1 bit. Shift last bit into register, if eight bits shifted update parallel - if bits all 1's set sync bit?
{
	uint8_t newStep = (DISK_VIA1_PinGetPIN_PB()&0x03);
	
	if (newStep!=lastStep)
	{
		int dir=1;
		if (((newStep==3) && (lastStep==0)) || ((newStep-lastStep)<0) )
			dir=-1;
		if ((newStep==0) && (lastStep==3))
			dir=1;

		lastStep=newStep;

		MoveHead(dir);
		printf("Step Request (new track %d%s) : Direction %s : %02X|%02X\n",curTrack>>1,(curTrack&1)?".5":"",(dir>0)?"in":"out",lastStep,newStep);
	}

	hackTime-=(1<<2);
	while (hackTime<=0)
	{
		hackTime+=diskStepTime;

		if ((DISK_VIA1_PinGetPIN_PB()&0x04)==0)
			return;

		// Bit Tick
		if (DISK_VIA1_PinGetPIN_CB2()==1)
		{	
			pBuffer<<=1;
			pBuffer&=0x3FE;
			lastBit=ReadBit();
			pBuffer|=ReadBit();
			RotateDisk();
			byteAvailable=0;
			syncMark=0x80;

			bufBits++;
			if (pBuffer==0x3FF)
			{
//				printf("SyncMark");
				syncMark=0x0;
				bufBits=0;		// Reset counter, we are processing a sync stream
			}
			if (bufBits==8)
			{
				// we've read a byte...
//				printf("headPosition : %08X : %02X,Byte Read = %02X\n", headPosition>>3,headPosition&7,pBuffer);
				byteAvailable=1;
				bufBits=0;
			}
			DISK_VIA1_PinSetPIN_PA(pBuffer);
		}
		else
		{
			if (bufBits==0)
			{
				pBuffer=DISK_VIA1_PinGetPIN_PA();
			}
			//printf("pbuffer=%02X\n",pBuffer);
			WriteBit((pBuffer&0x80)>>7);
			lastBit=ReadBit();
			pBuffer<<=1;
			pBuffer&=0xFE;
			RotateDisk();
			byteAvailable=0;

			bufBits++;
			if (bufBits==8)
			{
				// we've written a byte...
				byteAvailable=1;
				bufBits=0;
			}

		}

	}
	
	DISK_VIA1_PinSetPIN_PB(syncMark|(DISK_VIA1_PinGetPIN_PB()&0x6F)|0x10);
	
	if (byteAvailable && (DISK_VIA1_PinGetPIN_CA2()==1))
	{
		DISK_VIA1_PinSetPIN_CA1(0);
		DISK_PinSetPIN_SO(0);
	}
	else
	{
		DISK_VIA1_PinSetPIN_CA1(1);
		DISK_PinSetPIN_SO(1);
	}

	RecordPin(6,lastBit);
}

uint16_t DISK_Tick(uint8_t* clkP,uint8_t* atnP,uint8_t* datP)
{
	uint8_t clk,atn,dat;
	static uint16_t lastPC;
	uint16_t addr; 
	uint8_t pb=DISK_VIA0_PinGetPIN_PB()&0x9F;			// Clear drive number pins
	uint8_t pb4=(pb&0x10)>>4;

	atn=(*atnP)^1;
	clk=(*clkP)^1;
	dat=((*datP) & (1 ^ (atn ^ pb4)))^1;

	pb&=0x7A;
	pb|=clk<<2;
	pb|=atn<<7;
	pb|=dat;

	if (lastPA1!=pb)
	{
		//printf("%d %d\n",(pb&0x10)>>4,*atn);
		lastPA1=pb;
	}

	if (lastClk!=clk)
	{
		lastClk=clk;
	}

	DISK_VIA0_PinSetPIN_PB(pb);
	DISK_VIA0_PinSetPIN_CA1(atn);

	HandleDiskControllerHardware();

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
#if 0
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
#endif
	clk=(DISK_VIA0_PinGetPIN_PB()&0x08)>>3;
	dat=(DISK_VIA0_PinGetPIN_PB()&0x02)>>1;
	
	clk^=1;
	dat^=1;
	dat&=1 ^ (atn ^ pb4);
	
//	atn=(DISK_VIA0_PinGetPIN_PB()&0x10)>>4;
//	atn^=1;

//	*atnP=atn;
	*clkP=clk;
	*datP=dat;

	return lastPC;
}


///// D64 Loader

unsigned int GCR(unsigned int input)
{
	switch (input)
	{
		case 0:
			return 0x0A;
		case 1:
			return 0x0B;
		case 2:
			return 0x12;
		case 3:
			return 0x13;
		case 4:
			return 0x0E;
		case 5:
			return 0x0F;
		case 6:
			return 0x16;
		case 7:
			return 0x17;
		case 8:
			return 0x09;
		case 9:
			return 0x19;
		case 10:
			return 0x1A;
		case 11:
			return 0x1B;
		case 12:
			return 0x0D;
		case 13:
			return 0x1D;
		case 14:
			return 0x1E;
		case 15:
			return 0x15;

		default:
			printf("BARF - Invalid GCR code : %d\n",input);
			exit(-1);
	}
}

unsigned int diskBuilderPos=0;

void GCRWrite(unsigned char* data,int length)
{
	int a;
	unsigned int nibble;
	unsigned int gcrTemp;
	unsigned char toGCR[4];
	unsigned char GCRed[5];

	if ((length&3)!=0)
	{
		printf("BARF\n");
		exit(-1);
	}

	for (a=0;a<length>>2;a++)
	{
		toGCR[0]=data[(a<<2)+0];
		toGCR[1]=data[(a<<2)+1];
		toGCR[2]=data[(a<<2)+2];
		toGCR[3]=data[(a<<2)+3];

		// GCR it
		nibble=(toGCR[0]&0xF0)>>4;
		gcrTemp=GCR(nibble);
		nibble=(toGCR[0]&0x0F);
		gcrTemp<<=5;
		gcrTemp|=GCR(nibble);			// 10 bits in GCR

		GCRed[0]=gcrTemp>>2;
		gcrTemp&=0x3;
		gcrTemp<<=5;				// 2 bits in GCR

		nibble=(toGCR[1]&0xF0)>>4;
		gcrTemp|=GCR(nibble);
		nibble=(toGCR[1]&0x0F);
		gcrTemp<<=5;
		gcrTemp|=GCR(nibble);			// 12 bits in GCR

		GCRed[1]=gcrTemp>>4;
		gcrTemp&=0x0F;
		gcrTemp<<=5;				// 4 bits in GCR

		nibble=(toGCR[2]&0xF0)>>4;
		gcrTemp|=GCR(nibble);
		nibble=(toGCR[2]&0x0F);
		gcrTemp<<=5;
		gcrTemp|=GCR(nibble);			// 14 bits in GCR

		GCRed[2]=gcrTemp>>6;
		gcrTemp&=0x3F;
		gcrTemp<<=5;				// 6 bits in GCR
		
		nibble=(toGCR[3]&0xF0)>>4;
		gcrTemp|=GCR(nibble);
		nibble=(toGCR[3]&0x0F);
		gcrTemp<<=5;
		gcrTemp|=GCR(nibble);			// 16 bits in GCR
		
		GCRed[3]=gcrTemp>>8;
		gcrTemp&=0xFF;				// 8 bits in GCR

		GCRed[4]=gcrTemp;			// 0 bits in GCR

		// write 5 gcr bytes
		DiskData[diskBuilderPos++]=GCRed[0];
		DiskData[diskBuilderPos++]=GCRed[1];
		DiskData[diskBuilderPos++]=GCRed[2];
		DiskData[diskBuilderPos++]=GCRed[3];
		DiskData[diskBuilderPos++]=GCRed[4];
	}
}

int LoadD64(const char* filename)
{
	int a;
	int dataOffset=0;
	int trackLengthBytes;
	int trackNo;
	int sectorCnt;
	int sectorNo;

	int length;
	FILE *input = fopen(filename,"rb");

	fseek(input,0,SEEK_END);
	length = ftell(input);

	fseek(input,0,SEEK_SET);

	// just deal with 35 track files for now
	if (length!=174848)
	{
		printf("Not 35 track file .. or maybe has error codes\n");
		exit(-1);
	}

	// For each of the 35 tracks :
	
	for (trackNo=1;trackNo<36;trackNo++)
	{
		sectorCnt=17;
		trackLengthBytes=6250;
		if (trackNo<31)
		{
			sectorCnt=18;
			trackLengthBytes=6666;
		}
		if (trackNo<25)
		{
			sectorCnt=19;
			trackLengthBytes=7142;
		}
		if (trackNo<18)
		{
			sectorCnt=21;
			trackLengthBytes=7692;
		}

//		printf("Track %d : Sector Cnt %d\n",trackNo,sectorCnt);
//		printf("Theoretical Required Track Length = %d\n",sectorCnt*(353+9));
//		printf("Track Length = %d\n",trackLengthBytes);
		printf("Track %d Start = %08X\n",trackNo,dataOffset);
		dataOffset+=trackLengthBytes;
		for (sectorNo=0;sectorNo<sectorCnt;sectorNo++)
		{
			unsigned char byte;
			unsigned char header[8];
			unsigned char data[260];

			// Sync Header
			byte=0xFF;
			for (a=0;a<5;a++)
			{
				DiskData[diskBuilderPos++]=byte;
			}

			header[0]=0x08;
			header[2]=sectorNo;
			header[3]=trackNo;
			header[4]=0x6C;//0x42;//'B';
			header[5]=0x68;//0x42;//'B';
			header[1]=header[2]^header[3]^header[4]^header[5];
			header[6]=0x0F;
			header[7]=0x0F;

			// Header Info
			GCRWrite(header,8);

			// Header Gap
			byte=0x55;
			for (a=0;a<8;a++)
			{
				DiskData[diskBuilderPos++]=byte;
			}
			
			// Data Header
			byte=0xFF;
			for (a=0;a<5;a++)
			{
				DiskData[diskBuilderPos++]=byte;
			}

			data[0]=0x07;
			fread(&data[1],256,1,input);
			data[257]=data[1];
			for (a=2;a<257;a++)
			{
				data[257]^=data[a];
			}
			data[258]=0x0F;
			data[259]=0x0F;
			
			GCRWrite(data,260);

			// InterSector Gap
			byte=0x55;
			for (a=0;a<9;a++)
			{
				DiskData[diskBuilderPos++]=byte;
			}

			trackLengthBytes-=353+9;
			
			if (sectorNo == sectorCnt-1)
			{
				// Disk Remainder
//				printf("Dumping remaining bytes %d\n",trackLengthBytes);
				byte=0xFF;
				for (a=0;a<trackLengthBytes;a++)
				{
					DiskData[diskBuilderPos++]=byte;
				}

			}
		}
	}

	fclose(input);
}
