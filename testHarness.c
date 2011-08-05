/*
 *
 *
 *  Testing Harness for 8080 emulation
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

extern unsigned short PIN_A;
extern unsigned char PIN_D;
extern unsigned char PIN_READY;
extern unsigned char PIN_RESET;
extern unsigned char PIN_SYNC;
extern unsigned char PIN__WR;
extern unsigned short SP;
extern unsigned short PC;
extern unsigned short BC;
extern unsigned short DE;
extern unsigned short HL;
extern unsigned char A;
extern unsigned char FLAGS;
extern unsigned char IR;

void O1(void);
void O2(void);
void RESET(void);

extern unsigned char	SYNC_FETCH;
extern unsigned char	SYNC_MEM_READ;
extern unsigned char	SYNC_MEM_WRITE;
extern unsigned char	SYNC_STACK_READ;
extern unsigned char	SYNC_STACK_WRITE;
extern unsigned char	SYNC_INPUT;
extern unsigned char	SYNC_OUTPUT;
extern unsigned char	SYNC_INT_ACK;
extern unsigned char	SYNC_HALT_ACK;
extern unsigned char	SYNC_INT_ACK_HALTED;

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

void EXECUTE_CYCLES(unsigned char instruction,int cnt,char *named)
{
	int a;
	int readInstruction=0;
	static unsigned char SYNC_LATCH;

	for (a=0;a<cnt;a++)
	{
		O1();
		O2();

		if (a==0 && (PIN_SYNC!=1 || PIN_D!=SYNC_FETCH))
		{
			printf("Previous instruction took wrong number of cycles!\n");
			exit(1);
		}

		// Watch for SYNC pulse and TYPE and latch them

		if (PIN_SYNC)
		{
			SYNC_LATCH=PIN_D;
		}

		// CPU INPUT expects data to be available on T2 state so we can do that work on the PIN_SYNC itself

		if (PIN_SYNC)
		{
			if (SYNC_LATCH == SYNC_FETCH)
			{
				if (readInstruction)
				{
					printf("current instruction took wrong number of cycles!\n");
					exit(1);
				}
				readInstruction=1;
				printf("Reading next instruction - Address %04X <- %02X (%s)\n",PIN_A,instruction,named);
				PIN_D=instruction;
				PIN_READY=1;
			}
			else
			{
				if (SYNC_LATCH == SYNC_STACK_READ)
				{
					printf("Reading memory(stack) - Address %04X <- $55\n",PIN_A);
					PIN_D=0x55;
					PIN_READY=1;
				}
				else
				{
					if (SYNC_LATCH == SYNC_STACK_WRITE)
					{
						PIN_READY=1;
					}
					else
					{
						if (SYNC_LATCH == SYNC_MEM_READ)
						{
							printf("Reading memory - Address %04X <- 0\n",PIN_A);
							PIN_D=0;
							PIN_READY=1;
						}
						else
						{
							if (SYNC_LATCH == SYNC_MEM_WRITE)
							{
								PIN_READY=1;
							}
							else
							{
								if (SYNC_LATCH == SYNC_INPUT)
								{
									printf("Reading port %04X <- $AA\n",PIN_A);
									PIN_D=0xAA;
									PIN_READY=1;
								}
								else
								{
									if (SYNC_LATCH == SYNC_OUTPUT)
									{
										PIN_READY=1;
									}
									else
									{
										printf("PIN_D = %02X\n",PIN_D);
										exit(12);
									}
								}
							}
						}
					}
				}
			}

		}

		// CPU OUTPUT expects device to have signalled readyness to capture at state T2, but capture occurs at T3 (when _WR is low)

		if (PIN__WR == 0)
		{
			if (SYNC_LATCH == SYNC_STACK_WRITE)
			{
				printf("Writing memory(stack) - Address %04X <- %02X\n",PIN_A,PIN_D);
			}
			else
			{
				if (SYNC_LATCH == SYNC_MEM_WRITE)
				{
					printf("Writing memory - Address %04X <- %02X\n",PIN_A,PIN_D);
				}
				else
				{
					if (SYNC_LATCH == SYNC_OUTPUT)
					{
						printf("Writing port %04X <- %02X\n",PIN_A,PIN_D);
					}
				}
			}
		}
	}

	DUMP_REGISTERS();
}

int main(int argc,char**argv)
{
// First test, after 3 cycles and a reset pc should be ready to fetch from address 0
	PIN_RESET=1;
	RESET();
	O1();
	O2();
	O1();
	O2();
	O1();
	O2();
	PIN_RESET=0;
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

	EXECUTE_CYCLES(0x0F,4,"RRC");

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
	BC=0x8800;
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

	return 0;

}

