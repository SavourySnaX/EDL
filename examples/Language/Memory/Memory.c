/*

 Boiler plate for memory sample

*/

#include <stdint.h>

void PinSetMain(uint8_t);

int main(int argc,char** argv)
{
	int a;
	for (a=0;a<8;a++)
	{
		PinSetMain(0);
		PinSetMain(1);
	}
}
