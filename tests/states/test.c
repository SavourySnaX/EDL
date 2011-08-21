#include <stdio.h>
#include <stdint.h>

void PinSettest(uint8_t);

int main(int argc,char** argv)
{
	int a;
	for (a=0;a<10;a++)
	{
		PinSettest(0);
	}

	return 0;
}
