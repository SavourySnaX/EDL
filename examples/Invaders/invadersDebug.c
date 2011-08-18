
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

       	const char* mnemonic=List[AddressLookup(address)]();
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
