/*

 Boiler plate for hello world sample

*/

#include <stdint.h>

void PinSetMain(uint8_t);

int main(int argc,char** argv)
{
	PinSetMain(0);
	PinSetMain(0);		// Note how nothing is displayed

	PinSetMain(1);		//Positive edge trigger, causes Main HANDLER to fire

	PinSetMain(1);		// Note how nothing is displayed
	PinSetMain(0);
}
