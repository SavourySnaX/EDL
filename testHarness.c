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
extern unsigned short PC;
extern unsigned char A;
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
	printf("A = %02X\n",A);
	printf("PC= %04X\n",PC);
	printf("--------\n");
}

void EXECUTE_CYCLES(unsigned char instruction,int cnt)
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

		// Watch for SYNC pulse and TYPE

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
				printf("Reading next instruction - Address %04X <- %02X\n",PIN_A,instruction);
				PIN_D=instruction;
				PIN_READY=1;
			}
			else
			{
				if (SYNC_LATCH == SYNC_STACK_READ)
				{
					printf("Reading memory - Address %04X <- 00\n",PIN_A);
					PIN_D=0;
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
									printf("Reading port %04X <- $55\n",PIN_A);
									PIN_D=0x55;
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
				printf("Writing memory - Address %04X <- %02X\n",PIN_A,PIN_D);
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

	EXECUTE_CYCLES(0,4);		// NOP

	EXECUTE_CYCLES(0xFB,4);		// EI

	EXECUTE_CYCLES(0xF3,4);		// DI

	EXECUTE_CYCLES(0xF5,11);	// PUSH PSW

	EXECUTE_CYCLES(0xE3,18);	// XTHL

	EXECUTE_CYCLES(0xF1,10);	// POP PSW

	EXECUTE_CYCLES(0xF9,5);		// SPHL

	EXECUTE_CYCLES(0xEB,4);		// XCHG

	EXECUTE_CYCLES(0x2F,4);		// CMA

	EXECUTE_CYCLES(0x3F,4);		// CMC

	EXECUTE_CYCLES(0x37,4);		// STC

	EXECUTE_CYCLES(0xE9,5);		// PCHL

	EXECUTE_CYCLES(0xC9,10);	// RET

	EXECUTE_CYCLES(0x07,4);		// RLC

	EXECUTE_CYCLES(0x0F,4);		// RRC

	EXECUTE_CYCLES(0x17,4);		// RAL

	EXECUTE_CYCLES(0x1F,4);		// RAR

	EXECUTE_CYCLES(0x36,10);	// MVI M,$1

	EXECUTE_CYCLES(0x3A,13);	// LDA %$1%$0

	EXECUTE_CYCLES(0x32,13);	// STA %$1%$0

	EXECUTE_CYCLES(0x2A,16);	// LHLD %$1%$0

	EXECUTE_CYCLES(0x22,16);	// SHLD %$1%$0

	EXECUTE_CYCLES(0xC3,10);	// JMP %$1%$0

	EXECUTE_CYCLES(0xCD,17);	// CALL %$1%$0

	EXECUTE_CYCLES(0xDB,10);	// IN %$0

	EXECUTE_CYCLES(0xD3,10);	// OUT %$0

	EXECUTE_CYCLES(0,4);		// NOP

	return 0;

}

