/*
 *
 *
 *  Testing Harness for 8080 Step emulation
 *
 *
 *
 *
 *
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

extern uint16_t SP;
extern uint16_t PC;
extern uint16_t BC;
extern uint16_t DE;
extern uint16_t HL;
extern uint8_t A;
extern uint8_t FLAGS;
extern uint8_t IR;

extern uint8_t CYCLES;

void STEP(void);
void RESET(void);

void DUMP_REGISTERS()
{
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
	printf("PC= %04X\n",PC);
	printf("--------\n");
}

void Disassemble(unsigned char* memory,unsigned int address);

unsigned char someMemory[0x10000];
void EXECUTE_CYCLES(unsigned char instruction,int cnt,char *named)
{
	someMemory[PC]=instruction;
	Disassemble(someMemory,PC);
	STEP();
	if (cnt!=CYCLES)
	{
		printf("current instruction took wrong number of cycles! %s (%d != %d)\n",named,cnt,CYCLES);
		exit(1);
	}

	DUMP_REGISTERS();
}

void TEST_VIA_BINARY(const char *filename)
{
	FILE* infile = fopen(filename,"rb");

	if (infile==NULL)
	{
		exit(-3);
	}
	fread(someMemory,1,1262,infile);

	fclose(infile);

	RESET();

	printf("\n\n\n\n\n\n PERFORMING EXECUTION OF 8080TEST ROM \n\n\n\n\n\n\n");


	while (1==1)
	{
		DUMP_REGISTERS();
		Disassemble(someMemory,PC);
		if (someMemory[PC]==0x76)
		{
			if (A==0x55)
			{
				printf("Execution of ROM completed - Success\n");
				exit(0);
			}
			else
			{
				printf("Execution of ROM completed - Failure\n");
				exit(-1);
			}
		}
		STEP();
	}
}

int main(int argc,char**argv)
{
	RESET();

	if (PC!=0)
	{
		printf("ERROR: Reset failed test\n");
		exit(-1);
	}

	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x2F,4,"CMA");

	EXECUTE_CYCLES(0xFB,4,"EI");

	EXECUTE_CYCLES(0xF3,4,"DI");

	EXECUTE_CYCLES(0xF5,11,"PUSH PSW");

	EXECUTE_CYCLES(0xE3,18,"XTHL");

	EXECUTE_CYCLES(0xF1,10,"POP PSW");

	EXECUTE_CYCLES(0xF9,5,"SPHL");

	EXECUTE_CYCLES(0xEB,4,"XCHG");

	EXECUTE_CYCLES(0x3F,4,"CMC");

	EXECUTE_CYCLES(0x37,4,"STC");

	EXECUTE_CYCLES(0xE9,5,"PCHL");

	EXECUTE_CYCLES(0xC9,10,"RET");

	EXECUTE_CYCLES(0x07,4,"RLC");
	EXECUTE_CYCLES(0x00,4,"NOP");

	A = 0x01;
	EXECUTE_CYCLES(0x0F,4,"RRC");

	EXECUTE_CYCLES(0x37,4,"STC");
	EXECUTE_CYCLES(0x3F,4,"CMC");
	EXECUTE_CYCLES(0x17,4,"RAL");

	EXECUTE_CYCLES(0x1F,4,"RAR");

	EXECUTE_CYCLES(0x36,10,"MVI M");

	EXECUTE_CYCLES(0x3A,13,"LDA");

	EXECUTE_CYCLES(0x32,13,"STA");

	EXECUTE_CYCLES(0x2A,16,"LHLD");

	EXECUTE_CYCLES(0x22,16,"SHLD");

	EXECUTE_CYCLES(0xC3,10,"JMP");

	EXECUTE_CYCLES(0xCD,17,"CALL");

	EXECUTE_CYCLES(0xDB,10,"IN");

	EXECUTE_CYCLES(0xC7,11,"RST 0");
	EXECUTE_CYCLES(0xD7,11,"RST 0x10");

	EXECUTE_CYCLES(0xC5,11,"PUSH BC");
	EXECUTE_CYCLES(0xD5,11,"PUSH DE");

	EXECUTE_CYCLES(0xC1,10,"POP BC");
	EXECUTE_CYCLES(0xE1,10,"POP HL");

	EXECUTE_CYCLES(0xD3,10,"OUT");

	EXECUTE_CYCLES(0x47,5,"MOV B,A");
	EXECUTE_CYCLES(0x7D,5,"MOV A,L");

	EXECUTE_CYCLES(0x4E,7,"MOV C,M");

	EXECUTE_CYCLES(0x72,7,"MOV M,D");

	EXECUTE_CYCLES(0x06,7,"MVI B,#");

	EXECUTE_CYCLES(0x01,10,"LXI BC,#");
	EXECUTE_CYCLES(0x31,10,"LXI SP,#");

	EXECUTE_CYCLES(0x0A,7,"LDAX (BC)");
	
	EXECUTE_CYCLES(0x12,7,"STAX (DE)");

	EXECUTE_CYCLES(0xC2,10,"JNZ #");
	EXECUTE_CYCLES(0xCA,10,"JZ #");
	EXECUTE_CYCLES(0xD2,10,"JNC #");
	
	EXECUTE_CYCLES(0,4,"NOP");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x37,4,"STC");
	EXECUTE_CYCLES(0x3F,4,"CMC");

	EXECUTE_CYCLES(0xD4,17,"CNC #");
	EXECUTE_CYCLES(0xDC,11,"CC #");

	EXECUTE_CYCLES(0xD0,11,"RNC");
	EXECUTE_CYCLES(0xD8,5,"RC");

	EXECUTE_CYCLES(0xF6,7,"ORI #");
	EXECUTE_CYCLES(0,4,"NOP");

	A = 0x01;
	EXECUTE_CYCLES(0xB6,7,"ORA M");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0xB0,4,"ORA B");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0xB2,4,"ORA D");
	EXECUTE_CYCLES(0,4,"NOP");
	
	EXECUTE_CYCLES(0xAA,4,"XRA D");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0xAE,7,"XRA M");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0xEE,7,"XRI #");
	EXECUTE_CYCLES(0,4,"NOP");

	A = 0xF1;
	BC = 0xFF1F;
	EXECUTE_CYCLES(0xA1,4,"ANA C");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0xA6,7,"ANA M");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0xE6,7,"ANI #");
	EXECUTE_CYCLES(0,4,"NOP");

	A=0x88;
	BC=0x0800;
	EXECUTE_CYCLES(0x80,4,"ADD B");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x86,7,"ADD M");
	EXECUTE_CYCLES(0,4,"NOP");
	
	EXECUTE_CYCLES(0xC6,7,"ADI #");
	EXECUTE_CYCLES(0,4,"NOP");

	A=0x88;	
	EXECUTE_CYCLES(0x88,4,"ADC B");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x8E,7,"ADC M");
	EXECUTE_CYCLES(0,4,"NOP");
	
	EXECUTE_CYCLES(0xCE,7,"ACI #");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x04,5,"INR B");
	EXECUTE_CYCLES(0,4,"NOP");
	EXECUTE_CYCLES(0x34,10,"INR M");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x03,5,"INX BC");
	EXECUTE_CYCLES(0,4,"NOP");

	HL=0xFFFF;
	DE=0x0002;
	EXECUTE_CYCLES(0,4,"NOP");
	EXECUTE_CYCLES(0x19,10,"DAD DE");
	EXECUTE_CYCLES(0,4,"NOP");

	A=0x3A;
	EXECUTE_CYCLES(0x27,4,"DAA");
	EXECUTE_CYCLES(0,4,"NOP");
	
	EXECUTE_CYCLES(0xBF,4,"CMP A");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0xBE,7,"CMP M");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0xFE,7,"CPI #");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x3B,5,"DCX SP");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x25,5,"DCR H");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x35,10,"DCR M");
	EXECUTE_CYCLES(0,4,"NOP");
	
	A=0x88;
	BC=0x8900;
	EXECUTE_CYCLES(0x90,4,"SUB B");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x96,7,"SUB M");
	EXECUTE_CYCLES(0,4,"NOP");
	
	EXECUTE_CYCLES(0xD6,7,"SUI #");
	EXECUTE_CYCLES(0,4,"NOP");

	A=0x88;
	BC=0x8900;
	EXECUTE_CYCLES(0x98,4,"SBB B");
	EXECUTE_CYCLES(0,4,"NOP");

	EXECUTE_CYCLES(0x9E,7,"SBB M");
	EXECUTE_CYCLES(0,4,"NOP");
	
	EXECUTE_CYCLES(0xDE,7,"SBI #");
	EXECUTE_CYCLES(0,4,"NOP");

	// Unofficial NOP instructions 
	EXECUTE_CYCLES(0x08,4,"NOP");
	EXECUTE_CYCLES(0x10,4,"NOP");
	EXECUTE_CYCLES(0x18,4,"NOP");
	EXECUTE_CYCLES(0x20,4,"NOP");
	EXECUTE_CYCLES(0x28,4,"NOP");
	EXECUTE_CYCLES(0x30,4,"NOP");
	EXECUTE_CYCLES(0x38,4,"NOP");

	// Unofficial CALL instructions
	EXECUTE_CYCLES(0xDD,17,"CALL");
	EXECUTE_CYCLES(0xED,17,"CALL");
	EXECUTE_CYCLES(0xFD,17,"CALL");

	// Unofficial RET instruction
	EXECUTE_CYCLES(0xD9,10,"RET");
	
	// Unofficial JMP instruction
	EXECUTE_CYCLES(0xCB,10,"JMP");
	
	EXECUTE_CYCLES(0,4,"NOP");

	// Run a binary rom test

	TEST_VIA_BINARY("test.bin");

	return 0;

}

extern uint8_t *DIS_[256];

const char* decodeDisasm(unsigned char* memory,unsigned int address,int *count)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

       	const char* mnemonic=DIS_[memory[address]];
	const char* sPtr=mnemonic;
	char* dPtr=temporaryBuffer;

	*count=0;
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
			sprintf(sprintBuffer,"%02X",memory[address+offset]);
			while (*tPtr)
			{
				*dPtr++=*tPtr++;
			}
			doingDecode=0;
			*count++;
		}
		sPtr++;
	}
	*dPtr=0;

	return temporaryBuffer;
}

void Disassemble(unsigned char* memory,unsigned int address)
{
	int numBytes=0;
	int a;
	const char* retVal = decodeDisasm(memory,address,&numBytes);

	printf("%04X :",address);

	for (a=0;a<numBytes+1;a++)
	{
		printf("%02X ",memory[address+a]);
	}
	for (a=0;a<4-(numBytes+1);a++)
	{
		printf("   ");
	}
	printf("%s\n",retVal);
}

unsigned char GetByte(unsigned int addr)
{
	return someMemory[addr&0xFFFF];
}

unsigned char GetStackByte(unsigned int addr)
{
	return GetByte(addr&0xFFFF);
}

void SetByte(unsigned int addr,unsigned char byte)
{
	someMemory[addr&0xFFFF]=byte;
}

void SetStackByte(unsigned int addr,unsigned char byte)
{
	SetByte(addr&0xFFFF,byte);
}

unsigned char GetPort(unsigned short addr)
{
	return 0xAA;
}

void SetPort(unsigned short addr,unsigned char byte)
{
}

unsigned char GetInterruptVector()
{
	return 0x00;
}


