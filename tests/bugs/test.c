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
	printf("CBW expect 00000005, got %08X\n",result);
	if (result != 5)
		return 1;
	result = CBW(0x80);
	printf("CBW expect 0000FF80, got %08X\n",result);
	if (result != 0xFF80)
		return 1;
	printf("CWD expect 00000000, got %08X\n",result);
	result = CWD(0x0000);
	if (result != 0x0)
		return 1;
	result = CWD(0x8000);
	printf("CWD expect 0000FFFF, got %08X\n",result);
	if (result != 0xFFFF)
		return 1;

	seg = -1;
	cSize = 0;
	STEP();
	printf("bytes read expect 5, got %d\n",bytesRead);
	if (bytesRead != 5)
		return 1;
	printf("oSize expect 1, got %d\n",oSize);
	if (oSize != 1)
		return 1;
	printf("aSize expect 1, got %d\n",aSize);
	if (aSize != 1)
		return 1;
	printf("REPEAT expect 1, got %d\n",REPEAT);
	if (REPEAT != 1)
		return 1;
	printf("REPEATZ expect 1, got %d\n",REPEATZ);
	if (REPEATZ != 1)
		return 1;
	
	return 0;
}
