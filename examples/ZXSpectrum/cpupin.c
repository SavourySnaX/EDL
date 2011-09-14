/*
 * ZXSpectrum
 *
 *  Uses Z80 step core built in EDL
 *  Rest is C for now
 */

#include <stdio.h>
#include <stdint.h>

uint16_t PinGetPIN_A();
uint8_t PinGetPIN_D();
void PinSetPIN_D(uint8_t);
void PinSetPIN__CLK(uint8_t);
uint8_t PinGetPIN__M1();
uint8_t PinGetPIN__MREQ();
uint8_t PinGetPIN__WR();
uint8_t PinGetPIN__RD();
uint8_t PinGetPIN__RFSH();
uint8_t PinGetPIN__IORQ();
void PinSetPIN__WAIT(uint8_t);
void PinSetPIN__INT(uint8_t);
void PinSetPIN__NMI(uint8_t);
void PinSetPIN__RESET(uint8_t);
void PinSetPIN__BUSRQ(uint8_t);

extern uint16_t	AF;
extern uint16_t	BC;
extern uint16_t	DE;
extern uint16_t	HL;
extern uint16_t	_AF;
extern uint16_t	_BC;
extern uint16_t	_DE;
extern uint16_t	_HL;
extern uint16_t	IX;
extern uint16_t	IY;
extern uint16_t	PC;
extern uint16_t	SP;
extern uint16_t	IR;

extern uint8_t IM;
extern uint8_t IFF1;
extern uint8_t IFF2;

uint8_t GetByte(uint16_t addr);
void SetByte(uint16_t addr,uint8_t byte);
uint8_t GetPort(uint16_t port);
void SetPort(uint16_t port,uint8_t byte);
int Disassemble(unsigned int address,int registers);

void SHOW_PINS()
{
	printf("%s %s %s %s %s %s %04X %02X\n",
			PinGetPIN__M1()?"  ":"M1",
			PinGetPIN__MREQ()?"    ":"MREQ",
			PinGetPIN__RD()?"  ":"RD",
			PinGetPIN__WR()?"  ":"WR",
			PinGetPIN__RFSH()?"    ":"RFSH",
			PinGetPIN__IORQ()?"    ":"IORQ",
			PinGetPIN_A(),
			PinGetPIN_D());
}

void CPU_RESET()
{
	PinSetPIN__WAIT(1);
	PinSetPIN__INT(1);
	PinSetPIN__NMI(1);
	PinSetPIN__RESET(1);
	PinSetPIN__BUSRQ(1);

	PinSetPIN__RESET(0);
	PinSetPIN__CLK(1);
	PinSetPIN__CLK(0);
	PinSetPIN__CLK(1);
	PinSetPIN__CLK(0);
	PinSetPIN__CLK(1);
	PinSetPIN__CLK(0);
	PinSetPIN__RESET(1);
}


int CPU_STEP(int intClocks,int doDebug)
{
	if (intClocks)
	{
		PinSetPIN__INT(0);
	}
	else
	{
		PinSetPIN__INT(1);
	}

	PinSetPIN__CLK(1);
#if 0
	if ((!PinGetPIN__M1()) && PinGetPIN__MREQ())		// Catch all M1 cycles
	{
		// First cycle of a new instruction (DD,FD,CB,ED - all generate multiple m1 cycles - beware!)
		if (doDebug)
		{
			Disassemble(PinGetPIN_A(),1);
		}
	}
#endif
	if ((!PinGetPIN__M1()) && PinGetPIN__IORQ())
	{
		PinSetPIN_D(0xFF);
	}
	if ((!PinGetPIN__MREQ()) && (!PinGetPIN__RD()))
	{
		uint16_t addr = PinGetPIN_A();
		uint8_t  data = GetByte(addr);
		PinSetPIN_D(data);
	}
	if ((!PinGetPIN__IORQ()) && (!PinGetPIN__RD()))
	{
		uint16_t addr = PinGetPIN_A();
		uint8_t  data = GetPort(addr);
		PinSetPIN_D(data);
	}
	if ((!PinGetPIN__MREQ()) && (!PinGetPIN__WR()))
	{
		SetByte(PinGetPIN_A(),PinGetPIN_D());
	}
	if ((!PinGetPIN__IORQ()) && (!PinGetPIN__WR()))
	{
		SetPort(PinGetPIN_A(),PinGetPIN_D());
	}

	PinSetPIN__CLK(0);

	return 1;
}
