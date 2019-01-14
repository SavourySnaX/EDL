#include <stdio.h>
#include <stdint.h>

uint32_t CBW(uint32_t in);
uint16_t CWD(uint16_t in);

extern uint8_t cSize;
extern uint8_t aSize;
extern uint8_t oSize;
extern uint8_t seg;
extern uint8_t REPEAT;
extern uint8_t REPEATZ;

void STEP();

static int bytesRead = 0;
uint8_t GetNextByte()
{
	uint8_t broken[] = {0xF3,0x67,0x66,0x26,0xAD};
	return broken[bytesRead++];
}

int main(int argc,char** argv)
{
	uint32_t result = CBW(5);
	if (result != 5)
		return 1;
	result = CBW(0x80);
	if (result != 0xFF80)
		return 1;
	result = CWD(0x0000);
	if (result != 0x0)
		return 1;
	result = CWD(0x8000);
	if (result != 0xFFFF)
		return 1;

	seg = -1;
	cSize = 0;
	STEP();
	if (bytesRead != 5)
		return 1;
	if (oSize != 1)
		return 1;
	if (aSize != 1)
		return 1;
	if (REPEAT != 1)
		return 1;
	if (REPEATZ != 1)
		return 1;
	
	return 0;
}
