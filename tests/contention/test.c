#include <stdio.h>
#include <stdint.h>

void TEST(uint8_t);

uint8_t busExpected[3 * 4 * 2] = {	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
									0x55,0x55,0x55,0x55,0x55,0x55,
									0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,
									0x00,0x00,0x00,0x00,0x00 };

int nextBus = 0;
int testResult = 0;
void BusTap(uint8_t bits, uint32_t val, const char* name, uint32_t decodeWidth, uint8_t last)
{
	if (busExpected[nextBus] != val)
	{
		printf("Bus Mismatch : expected %02X, got %02X\n", busExpected[nextBus], val);
		testResult = 1;
	}
	nextBus++;
}

int main(int argc,char** argv)
{
	int a;
	for (a=0;a<3*4*2;a++)
	{
		TEST(a&1);
	}

	return testResult;
}
