#include <stdio.h>
#include <stdint.h>

void TEST(uint8_t,uint8_t);

uint8_t busExpected[4 * 2] = {	0,0,0,1,1,0,1,1 };

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
	
	TEST(0, 0);
	TEST(0, 1);
	TEST(1, 0);
	TEST(1, 1);

	return 0;
}
