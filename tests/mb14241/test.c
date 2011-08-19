/*
 *
 * Verify mb14241 shifter implementation
 *
*/

#include <stdio.h>
#include <stdint.h>

void PinSetINPUTS(uint8_t);
uint8_t PinGetOUTPUTS();

void PinSetLATCH_SHIFT(uint8_t);
void PinSetLATCH_DATA(uint8_t);

void UpdateShiftCount(uint8_t data);
void UpdateShiftData(uint8_t data);
uint8_t GetAnswer();

int main(int argc,char** argv)
{
	int a,b,c;
	for (a=0;a<8;a++)
	{
		PinSetINPUTS(a);
		PinSetLATCH_SHIFT(0);
		PinSetLATCH_SHIFT(1);		// Clock shift count
		UpdateShiftCount(a);

		for (b=0;b<256;b++)
		{
			for (c=0;c<256;c++)
			{
				PinSetINPUTS(b);
				UpdateShiftData(b);
				PinSetLATCH_DATA(0);
				PinSetLATCH_DATA(1);
				PinSetINPUTS(c);
				UpdateShiftData(c);
				PinSetLATCH_DATA(0);
				PinSetLATCH_DATA(1);		// Clock a full 15 bit word through

				if (PinGetOUTPUTS() != GetAnswer())
				{
					printf("Mismatch\n");
					return -1;
				}
			}
		}
	}

	return 0;
}


uint8_t shiftCount=0;
uint16_t shiftData=0;

void UpdateShiftCount(uint8_t data)
{
	shiftCount=~data & 0x07;
}

void UpdateShiftData(uint8_t data)
{
	shiftData=(shiftData>>8)  | ((uint16_t)data << 7);
}

uint8_t GetAnswer()
{
	return shiftData >> shiftCount;
}

