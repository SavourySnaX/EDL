#include <stdio.h>
#include <stdint.h>

void PinSetPIN_A(uint8_t);
uint8_t PinGetPIN_D();

int main(int argc,char** argv)
{
	uint8_t expected[8] = { 1,0,1,1,1,0,1,1 };

	int a;
	PinSetPIN_A(0);
	PinSetPIN_A(0);

	for (a=0;a<4;a++)
	{
		PinSetPIN_A(1);
		if ((PinGetPIN_D() & 1) != expected[a * 2 + 0])
		{
			printf("Expected %d, but got %d, 0->1 transition",expected[a*2+0],(PinGetPIN_D()&1));
			return 1;
		}
		PinSetPIN_A(0);
		if ((PinGetPIN_D() & 1) != expected[a * 2 + 1])
		{
			printf("Expected %d, but got %d, 1->0 transition",expected[a*2+1],(PinGetPIN_D()&1));
			return 1;
		}
	}

	return 0;
}
