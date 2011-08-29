
#include <stdint.h>
#include <stdio.h>

extern uint16_t SP;
extern uint16_t PC;
extern uint16_t BC;
extern uint16_t DE;
extern uint16_t HL;
extern uint8_t A;
extern uint8_t FLAGS;


extern unsigned char Ram[0x2000];
extern unsigned char Rom[0x2000];

extern uint8_t *DIS_[256];

void DUMP_REGISTERS()
{
#if 1
	printf("--------\n");
	printf("FLAGS = S  Z  0  AC 0  P  1  CY\n");
	printf("        %s  %s  %s  %s  %s  %s  %s  %s\n",
			FLAGS&0x80 ? "1" : "0",
			FLAGS&0x40 ? "1" : "0",
			FLAGS&0x20 ? "1" : "0",
			FLAGS&0x10 ? "1" : "0",
			FLAGS&0x08 ? "1" : "0",
			FLAGS&0x04 ? "1" : "0",
			FLAGS&0x02 ? "1" : "0",
			FLAGS&0x01 ? "1" : "0");
	printf("A = %02X\n",A);
	printf("BC= %04X\n",BC);
	printf("DE= %04X\n",DE);
	printf("HL= %04X\n",HL);
	printf("SP= %04X\n",SP);
	printf("--------\n");
#else
    printf("PC : %04X\n",PC-1);
    printf("B : %04X\n",BC>>8);
    printf("C : %04X\n",BC&0xFF);
    printf("D : %04X\n",DE>>8);
    printf("E : %04X\n",DE&0xFF);
    printf("H : %04X\n",HL>>8);
    printf("L : %04X\n",HL&0xFF);
    printf("A : %04X\n",A);
    printf("F : %04X\n",FLAGS);
    printf("SP : %04X\n",SP);
#endif
}

unsigned char AddressLookup(unsigned int address)
{
	if (address&0x2000)
		return Ram[address&0x1FFF];
	else
		return Rom[address&0x1FFF];
}

const char* decodeDisasm(unsigned int address,int *count)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

       	const char* mnemonic=DIS_[AddressLookup(address)];
	const char* sPtr=mnemonic;
	char* dPtr=temporaryBuffer;
	int counting = 0;
	int doingDecode=0;

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
			int offset=*sPtr-'0';
			sprintf(sprintBuffer,"%02X",AddressLookup(address+offset));
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

void Disassemble(unsigned int address)
{
	int a;
	int numBytes=0;
	const char* retVal = decodeDisasm(address,&numBytes);

	DUMP_REGISTERS();
	printf("%04X :",address);

	for (a=0;a<numBytes+1;a++)
	{
		printf("%02X ",AddressLookup(address+a));
	}
	for (a=0;a<4-(numBytes+1);a++)
	{
		printf("   ");
	}
	printf("%s\n",retVal);
}

/*
void Disassemble(unsigned int address)
{
	int a;
	//for (a=0x40;a<0x80;a++)
	//	List[a]();
	DUMP_REGISTERS();
	
	printf("%04X %02X %02X %02X: ",address,Rom[address&0x1FFF],Rom[(1+address)&0x1FFF],Rom[(address+2)&0x1FFF]);
	if (address&0x2000)
		List[Ram[address&0x1FFF]]();
	else
		List[Rom[address&0x1FFF]]();
}*/
