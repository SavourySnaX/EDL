/*
 * ZXSpectrum
 *
 *  Uses Z80 step core built in EDL
 *  Rest is C for now
 */

#include <stdio.h>
#include <stdint.h>

void STEP(void);
void RESET(void);
void INTERRUPT(uint8_t);

extern uint8_t CYCLES;

void CPU_RESET()
{
	RESET();
}

int CPU_STEP(int intClocks,int doDebug)
{
	if (intClocks)
	{
		INTERRUPT(0xFF);
	
		if (CYCLES==0)
		{
			STEP();
		}
	}
	else
	{
		STEP();
	}

	return CYCLES;
}
