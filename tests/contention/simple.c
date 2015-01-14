#include <stdio.h>
#include <stdint.h>

void PinSetPIN_A(uint8_t);
uint8_t PinGetPIN_D();

int main(int argc,char** argv)
{
	int a;
	PinSetPIN_A(0);
	PinSetPIN_A(0);

	for (a=0;a<4;a++)
	{
		PinSetPIN_A(1);
		printf("PIN_D == %d\n",PinGetPIN_D()&1);
		PinSetPIN_A(0);
		printf("PIN_D == %d\n",PinGetPIN_D()&1);
	}

	return 0;
}
