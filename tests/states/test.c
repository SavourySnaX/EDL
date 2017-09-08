#include <stdio.h>
#include <stdint.h>

void PinSettest(uint8_t);

extern uint8_t stateOrder[16];
extern uint8_t bingoCnt;
extern uint8_t bangoCnt;

int main(int argc,char** argv)
{
	uint8_t orderCheck[16] = { 0,1,2,3,4,5,8,9,6,7 };
	int a;
	for (a=0;a<10;a++)
	{
		PinSettest(0);
	}

	if (bingoCnt != 5)
		return 1;
	if (bangoCnt != 2)
		return 1;

	for (a = 0; a < 10; a++)
	{
		if (stateOrder[a] != orderCheck[a])
		{
			return 1;
		}
	}

	return 0;
}
