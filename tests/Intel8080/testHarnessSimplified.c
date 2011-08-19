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

void PinSetSTEP(uint8_t);
void PinSetRESET(uint8_t);

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
	PinSetSTEP(1);			// doesn't matter what value is passed, will trigger STEP on any value
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

	PinSetRESET(1);			// doesn't matter what value is passed, will trigger RESET on any value

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
		PinSetSTEP(1);
	}
}

int main(int argc,char**argv)
{
	PinSetRESET(1);

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


extern const char* DIS_0();
extern const char* DIS_1();
extern const char* DIS_2();
extern const char* DIS_3();
extern const char* DIS_4();
extern const char* DIS_5();
extern const char* DIS_6();
extern const char* DIS_7();
extern const char* DIS_8();
extern const char* DIS_9();
extern const char* DIS_A();
extern const char* DIS_B();
extern const char* DIS_C();
extern const char* DIS_D();
extern const char* DIS_E();
extern const char* DIS_F();
extern const char* DIS_10();
extern const char* DIS_11();
extern const char* DIS_12();
extern const char* DIS_13();
extern const char* DIS_14();
extern const char* DIS_15();
extern const char* DIS_16();
extern const char* DIS_17();
extern const char* DIS_18();
extern const char* DIS_19();
extern const char* DIS_1A();
extern const char* DIS_1B();
extern const char* DIS_1C();
extern const char* DIS_1D();
extern const char* DIS_1E();
extern const char* DIS_1F();
extern const char* DIS_20();
extern const char* DIS_21();
extern const char* DIS_22();
extern const char* DIS_23();
extern const char* DIS_24();
extern const char* DIS_25();
extern const char* DIS_26();
extern const char* DIS_27();
extern const char* DIS_28();
extern const char* DIS_29();
extern const char* DIS_2A();
extern const char* DIS_2B();
extern const char* DIS_2C();
extern const char* DIS_2D();
extern const char* DIS_2E();
extern const char* DIS_2F();
extern const char* DIS_30();
extern const char* DIS_31();
extern const char* DIS_32();
extern const char* DIS_33();
extern const char* DIS_34();
extern const char* DIS_35();
extern const char* DIS_36();
extern const char* DIS_37();
extern const char* DIS_38();
extern const char* DIS_39();
extern const char* DIS_3A();
extern const char* DIS_3B();
extern const char* DIS_3C();
extern const char* DIS_3D();
extern const char* DIS_3E();
extern const char* DIS_3F();
extern const char* DIS_40();
extern const char* DIS_41();
extern const char* DIS_42();
extern const char* DIS_43();
extern const char* DIS_44();
extern const char* DIS_45();
extern const char* DIS_46();
extern const char* DIS_47();
extern const char* DIS_48();
extern const char* DIS_49();
extern const char* DIS_4A();
extern const char* DIS_4B();
extern const char* DIS_4C();
extern const char* DIS_4D();
extern const char* DIS_4E();
extern const char* DIS_4F();
extern const char* DIS_50();
extern const char* DIS_51();
extern const char* DIS_52();
extern const char* DIS_53();
extern const char* DIS_54();
extern const char* DIS_55();
extern const char* DIS_56();
extern const char* DIS_57();
extern const char* DIS_58();
extern const char* DIS_59();
extern const char* DIS_5A();
extern const char* DIS_5B();
extern const char* DIS_5C();
extern const char* DIS_5D();
extern const char* DIS_5E();
extern const char* DIS_5F();
extern const char* DIS_60();
extern const char* DIS_61();
extern const char* DIS_62();
extern const char* DIS_63();
extern const char* DIS_64();
extern const char* DIS_65();
extern const char* DIS_66();
extern const char* DIS_67();
extern const char* DIS_68();
extern const char* DIS_69();
extern const char* DIS_6A();
extern const char* DIS_6B();
extern const char* DIS_6C();
extern const char* DIS_6D();
extern const char* DIS_6E();
extern const char* DIS_6F();
extern const char* DIS_70();
extern const char* DIS_71();
extern const char* DIS_72();
extern const char* DIS_73();
extern const char* DIS_74();
extern const char* DIS_75();
extern const char* DIS_76();
extern const char* DIS_77();
extern const char* DIS_78();
extern const char* DIS_79();
extern const char* DIS_7A();
extern const char* DIS_7B();
extern const char* DIS_7C();
extern const char* DIS_7D();
extern const char* DIS_7E();
extern const char* DIS_7F();
extern const char* DIS_80();
extern const char* DIS_81();
extern const char* DIS_82();
extern const char* DIS_83();
extern const char* DIS_84();
extern const char* DIS_85();
extern const char* DIS_86();
extern const char* DIS_87();
extern const char* DIS_88();
extern const char* DIS_89();
extern const char* DIS_8A();
extern const char* DIS_8B();
extern const char* DIS_8C();
extern const char* DIS_8D();
extern const char* DIS_8E();
extern const char* DIS_8F();
extern const char* DIS_90();
extern const char* DIS_91();
extern const char* DIS_92();
extern const char* DIS_93();
extern const char* DIS_94();
extern const char* DIS_95();
extern const char* DIS_96();
extern const char* DIS_97();
extern const char* DIS_98();
extern const char* DIS_99();
extern const char* DIS_9A();
extern const char* DIS_9B();
extern const char* DIS_9C();
extern const char* DIS_9D();
extern const char* DIS_9E();
extern const char* DIS_9F();
extern const char* DIS_A0();
extern const char* DIS_A1();
extern const char* DIS_A2();
extern const char* DIS_A3();
extern const char* DIS_A4();
extern const char* DIS_A5();
extern const char* DIS_A6();
extern const char* DIS_A7();
extern const char* DIS_A8();
extern const char* DIS_A9();
extern const char* DIS_AA();
extern const char* DIS_AB();
extern const char* DIS_AC();
extern const char* DIS_AD();
extern const char* DIS_AE();
extern const char* DIS_AF();
extern const char* DIS_B0();
extern const char* DIS_B1();
extern const char* DIS_B2();
extern const char* DIS_B3();
extern const char* DIS_B4();
extern const char* DIS_B5();
extern const char* DIS_B6();
extern const char* DIS_B7();
extern const char* DIS_B8();
extern const char* DIS_B9();
extern const char* DIS_BA();
extern const char* DIS_BB();
extern const char* DIS_BC();
extern const char* DIS_BD();
extern const char* DIS_BE();
extern const char* DIS_BF();
extern const char* DIS_C0();
extern const char* DIS_C1();
extern const char* DIS_C2();
extern const char* DIS_C3();
extern const char* DIS_C4();
extern const char* DIS_C5();
extern const char* DIS_C6();
extern const char* DIS_C7();
extern const char* DIS_C8();
extern const char* DIS_C9();
extern const char* DIS_CA();
extern const char* DIS_CB();
extern const char* DIS_CC();
extern const char* DIS_CD();
extern const char* DIS_CE();
extern const char* DIS_CF();
extern const char* DIS_D0();
extern const char* DIS_D1();
extern const char* DIS_D2();
extern const char* DIS_D3();
extern const char* DIS_D4();
extern const char* DIS_D5();
extern const char* DIS_D6();
extern const char* DIS_D7();
extern const char* DIS_D8();
extern const char* DIS_D9();
extern const char* DIS_DA();
extern const char* DIS_DB();
extern const char* DIS_DC();
extern const char* DIS_DD();
extern const char* DIS_DE();
extern const char* DIS_DF();
extern const char* DIS_E0();
extern const char* DIS_E1();
extern const char* DIS_E2();
extern const char* DIS_E3();
extern const char* DIS_E4();
extern const char* DIS_E5();
extern const char* DIS_E6();
extern const char* DIS_E7();
extern const char* DIS_E8();
extern const char* DIS_E9();
extern const char* DIS_EA();
extern const char* DIS_EB();
extern const char* DIS_EC();
extern const char* DIS_ED();
extern const char* DIS_EE();
extern const char* DIS_EF();
extern const char* DIS_F0();
extern const char* DIS_F1();
extern const char* DIS_F2();
extern const char* DIS_F3();
extern const char* DIS_F4();
extern const char* DIS_F5();
extern const char* DIS_F6();
extern const char* DIS_F7();
extern const char* DIS_F8();
extern const char* DIS_F9();
extern const char* DIS_FA();
extern const char* DIS_FB();
extern const char* DIS_FC();
extern const char* DIS_FD();
extern const char* DIS_FE();
extern const char* DIS_FF();
typedef const char* (*fptr)();
fptr List[256]={
DIS_0,
DIS_1,
DIS_2,
DIS_3,
DIS_4,
DIS_5,
DIS_6,
DIS_7,
DIS_8,
DIS_9,
DIS_A,
DIS_B,
DIS_C,
DIS_D,
DIS_E,
DIS_F,
DIS_10,
DIS_11,
DIS_12,
DIS_13,
DIS_14,
DIS_15,
DIS_16,
DIS_17,
DIS_18,
DIS_19,
DIS_1A,
DIS_1B,
DIS_1C,
DIS_1D,
DIS_1E,
DIS_1F,
DIS_20,
DIS_21,
DIS_22,
DIS_23,
DIS_24,
DIS_25,
DIS_26,
DIS_27,
DIS_28,
DIS_29,
DIS_2A,
DIS_2B,
DIS_2C,
DIS_2D,
DIS_2E,
DIS_2F,
DIS_30,
DIS_31,
DIS_32,
DIS_33,
DIS_34,
DIS_35,
DIS_36,
DIS_37,
DIS_38,
DIS_39,
DIS_3A,
DIS_3B,
DIS_3C,
DIS_3D,
DIS_3E,
DIS_3F,
DIS_40,
DIS_41,
DIS_42,
DIS_43,
DIS_44,
DIS_45,
DIS_46,
DIS_47,
DIS_48,
DIS_49,
DIS_4A,
DIS_4B,
DIS_4C,
DIS_4D,
DIS_4E,
DIS_4F,
DIS_50,
DIS_51,
DIS_52,
DIS_53,
DIS_54,
DIS_55,
DIS_56,
DIS_57,
DIS_58,
DIS_59,
DIS_5A,
DIS_5B,
DIS_5C,
DIS_5D,
DIS_5E,
DIS_5F,
DIS_60,
DIS_61,
DIS_62,
DIS_63,
DIS_64,
DIS_65,
DIS_66,
DIS_67,
DIS_68,
DIS_69,
DIS_6A,
DIS_6B,
DIS_6C,
DIS_6D,
DIS_6E,
DIS_6F,
DIS_70,
DIS_71,
DIS_72,
DIS_73,
DIS_74,
DIS_75,
DIS_76,
DIS_77,
DIS_78,
DIS_79,
DIS_7A,
DIS_7B,
DIS_7C,
DIS_7D,
DIS_7E,
DIS_7F,
DIS_80,
DIS_81,
DIS_82,
DIS_83,
DIS_84,
DIS_85,
DIS_86,
DIS_87,
DIS_88,
DIS_89,
DIS_8A,
DIS_8B,
DIS_8C,
DIS_8D,
DIS_8E,
DIS_8F,
DIS_90,
DIS_91,
DIS_92,
DIS_93,
DIS_94,
DIS_95,
DIS_96,
DIS_97,
DIS_98,
DIS_99,
DIS_9A,
DIS_9B,
DIS_9C,
DIS_9D,
DIS_9E,
DIS_9F,
DIS_A0,
DIS_A1,
DIS_A2,
DIS_A3,
DIS_A4,
DIS_A5,
DIS_A6,
DIS_A7,
DIS_A8,
DIS_A9,
DIS_AA,
DIS_AB,
DIS_AC,
DIS_AD,
DIS_AE,
DIS_AF,
DIS_B0,
DIS_B1,
DIS_B2,
DIS_B3,
DIS_B4,
DIS_B5,
DIS_B6,
DIS_B7,
DIS_B8,
DIS_B9,
DIS_BA,
DIS_BB,
DIS_BC,
DIS_BD,
DIS_BE,
DIS_BF,
DIS_C0,
DIS_C1,
DIS_C2,
DIS_C3,
DIS_C4,
DIS_C5,
DIS_C6,
DIS_C7,
DIS_C8,
DIS_C9,
DIS_CA,
DIS_CB,
DIS_CC,
DIS_CD,
DIS_CE,
DIS_CF,
DIS_D0,
DIS_D1,
DIS_D2,
DIS_D3,
DIS_D4,
DIS_D5,
DIS_D6,
DIS_D7,
DIS_D8,
DIS_D9,
DIS_DA,
DIS_DB,
DIS_DC,
DIS_DD,
DIS_DE,
DIS_DF,
DIS_E0,
DIS_E1,
DIS_E2,
DIS_E3,
DIS_E4,
DIS_E5,
DIS_E6,
DIS_E7,
DIS_E8,
DIS_E9,
DIS_EA,
DIS_EB,
DIS_EC,
DIS_ED,
DIS_EE,
DIS_EF,
DIS_F0,
DIS_F1,
DIS_F2,
DIS_F3,
DIS_F4,
DIS_F5,
DIS_F6,
DIS_F7,
DIS_F8,
DIS_F9,
DIS_FA,
DIS_FB,
DIS_FC,
DIS_FD,
DIS_FE,
DIS_FF};

const char* decodeDisasm(unsigned char* memory,unsigned int address,int *count)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

       	const char* mnemonic=List[memory[address]]();
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

unsigned char GetByte(unsigned short addr)
{
	return someMemory[addr];
}

unsigned char GetStackByte(unsigned short addr)
{
	return GetByte(addr);
}

void SetByte(unsigned short addr,unsigned char byte)
{
	someMemory[addr]=byte;
}

void SetStackByte(unsigned short addr,unsigned char byte)
{
	SetByte(addr,byte);
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


