#include <stdio.h>
#include <stdint.h>

void TEST(uint8_t);

int main(int argc,char** argv)
{
	int a;
	for (a=0;a<3*4*2;a++)
	{
		TEST(a&1);
	}

	return 0;
}
