#include <stdio.h>
#include <stdint.h>

void PinSettest(uint8_t);

extern uint8_t stateOrder[16];
extern uint32_t L32;

int main(int argc,char** argv)
{
	PinSettest(0);

    if (L32 != 0xAABBFFCC)
        return 1;

    return 0;
}
