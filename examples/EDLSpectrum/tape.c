#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TSTATES_PER_ROW	224
#define ROWS_PER_VBLANK	312

// Tape handling
uint8_t		tapePaused = 1;
uint32_t	tapePos=0;
uint32_t	tapeTCounter=0;
uint8_t		tapeLevel=0;
uint8_t*	tapeFile=NULL;
uint32_t	tapeLength=0;
uint32_t	tapePhase=0;
uint32_t	tapeFormat=0;

uint32_t	tapeDataLength;
uint8_t		tapeDataBit;
uint8_t		tapeDataBitCnt;
uint16_t	tapePilotLength;
uint32_t	tapePilotPulseLength;

uint16_t	tapeParamPilotLength;
uint16_t	tapeParamLoopCounter;
uint32_t	tapeParamLoopMarker;
uint32_t	tapeParamPilotPulseLength;
uint32_t	tapeParamSync1Length;
uint32_t	tapeParamSync2Length;
uint32_t	tapeParamOneLength;
uint32_t	tapeParamZeroLength;
uint32_t	tapeParamDelayLength;
uint8_t		tapeParamUsedBits;
uint32_t	tapeParamLastCase;

void PauseTape()
{
	tapePaused = 1;
}

void PlayTape()
{
	tapePaused = 0;
}

unsigned char *qLoad(const char *romName,uint32_t *length)
{
	FILE *inRom=NULL;
	unsigned char *romData;
	*length=0;

	if (fopen_s(&inRom, romName, "rb"))
		inRom = NULL;
	if (!inRom)
	{
		return 0;
	}
	fseek(inRom,0,SEEK_END);
	*length = ftell(inRom);

	fseek(inRom,0,SEEK_SET);
	romData = (unsigned char *)malloc(*length);
	if (romData)
	{
		if ( *length != fread(romData,1,*length,inRom))
		{
			free(romData);
			*length=0;
			romData=0;
		}
	}
	fclose(inRom);

	return romData;
}

int CheckTAP()
{
	uint16_t blockLength;
	uint32_t curPos=0;

	while (1)
	{
		blockLength=tapeFile[curPos++];
		if (curPos>=tapeLength)
		{
			return 0;
		}
		blockLength|=tapeFile[curPos++]<<8;
		if (curPos>=tapeLength)
		{
			return 0;
		}
		if (blockLength==0)
		{
			return 0;	// Not likely to be a .TAP
		}
		if (tapeFile[curPos]<0x80)
		{
			// Header block
			curPos+=19;
			if (curPos==tapeLength)
			{
				return 1;
			}
			if (curPos>tapeLength)
			{
				return 0;
			}
		}
		else
		{
			curPos+=blockLength;
			if (curPos==tapeLength)
			{
				return 1;
			}
			if (curPos>tapeLength)
			{
				return 0;
			}
		}
	}
}

int CheckTZX()
{
	uint32_t blockLength;
	uint32_t curPos=0;

	// Check header
	if (strncmp(tapeFile,"ZXTape!",7)!=0)
	{
		return 0;
	}
	if (tapeFile[7]!=0x1A)
	{
		return 0;
	}

	printf("TZX: Revision : %d.%d\n",tapeFile[8],tapeFile[9]);

	// Quickly check the block Ids - if an unsupported block is found, exit

	curPos=10;
	while (1)
	{
		uint8_t blockId;
		if (curPos>tapeLength)
		{
			printf("Error processing TZX, aborting\n");
			exit(-1);
		}
		if (curPos==tapeLength)
		{
			break;
		}

		blockId=tapeFile[curPos++];
		switch (blockId)
		{
			case 0x10:
				printf("Standard Block\n");
				curPos+=2;
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				curPos+=blockLength;
				break;
			case 0x11:
				printf("Speed Block\n");
				curPos+=15;
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				blockLength|=tapeFile[curPos++]<<16;
				curPos+=blockLength;
				break;
			case 0x12:
				printf("Pure Tone\n");
				curPos+=4;
				break;
			case 0x13:
				printf("Pulse Sequence\n");
				curPos+=tapeFile[curPos]*2+1;
				break;
			case 0x14:
				printf("Pure Data\n");
				curPos+=7;
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				blockLength|=tapeFile[curPos++]<<16;
				curPos+=blockLength;
				break;
			case 0x20:
				printf("Silence/Stop Tape\n");
				curPos+=2;
				break;
			case 0x21:
				printf("Group Start\n");
				curPos+=tapeFile[curPos]+1;
				break;
			case 0x22:
				printf("Group End\n");
				break;
			case 0x24:
				printf("Loop Start\n");
				curPos+=2;
				break;
			case 0x25:
				printf("Loop End\n");
				break;
			case 0x2A:
				printf("Stop Tape (48K)\n");
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				blockLength|=tapeFile[curPos++]<<16;
				blockLength|=tapeFile[curPos++]<<24;
				curPos+=blockLength;
				break;
			case 0x30:
				printf("Text Description\n");
				curPos+=tapeFile[curPos]+1;
				break;
			case 0x32:
				printf("Archive Info\n");
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				curPos+=blockLength;
				break;
			case 0x35:
				printf("Custom Info\n");
				curPos+=16;
				blockLength=tapeFile[curPos++];
				blockLength|=tapeFile[curPos++]<<8;
				blockLength|=tapeFile[curPos++]<<16;
				blockLength|=tapeFile[curPos++]<<24;
				curPos+=blockLength;
				break;
			default:
				printf("Unhandled ID%02X\n",blockId);
				exit(-1);
				break;
		}
	}

	return 1;
}

void LoadTapeFormat(const char* filename)		// Currently handles (raw audio, .TAP (z80) & .tzx formats (tzx implementation incomplete))
{
	if (filename==NULL)
	{
		tapeFormat=99;
		return;
	}

	tapeFile=qLoad(filename,&tapeLength);
	if (!tapeFile)
	{
		printf("Failed to load file - %s (ignoring)\n",filename);
		tapeFormat=99;
		return;
	}

	tapePos=0;
	tapeLevel=0;
	tapePhase=0;
	tapeTCounter=0;

	tapeFormat=0;
	if (CheckTAP())
	{
		tapeFormat=1;
	}
	if (CheckTZX())
	{
		tapePos=10;
		tapeFormat=2;
	}
}

uint8_t GetNextBlockTZX()
{
	uint32_t blockLength;
	uint8_t blockId;
	
	while (1)
	{
		if (tapePos>=tapeLength)
		{
			tapePos=10;
		}

		blockId=tapeFile[tapePos++];
		switch (blockId)
		{
			case 0x10:
				printf("Standard\n");
				tapeParamDelayLength=tapeFile[tapePos++];
				tapeParamDelayLength|=tapeFile[tapePos++]<<8;
				tapeDataLength=tapeFile[tapePos++];
				tapeDataLength|=tapeFile[tapePos++]<<8;
				tapeParamPilotPulseLength=2168;
				if (tapeFile[tapePos]<0x80)
				{
					// Header Pilot Pulse
					tapeParamPilotLength=8063;
				}
				else
				{
					// data Pilot Pulse
					tapeParamPilotLength=3223;
				}
				tapeParamSync1Length=667;
				tapeParamSync2Length=735;
				tapeParamOneLength=1710;
				tapeParamZeroLength=855;
				tapeParamLastCase=8;
				tapeParamUsedBits=8;
				return 1;
			case 0x11:
				printf("Speed\n");
				tapeParamPilotPulseLength=tapeFile[tapePos++];
				tapeParamPilotPulseLength|=tapeFile[tapePos++]<<8;
				tapeParamSync1Length=tapeFile[tapePos++];
				tapeParamSync1Length|=tapeFile[tapePos++]<<8;
				tapeParamSync2Length=tapeFile[tapePos++];
				tapeParamSync2Length|=tapeFile[tapePos++]<<8;
				tapeParamZeroLength=tapeFile[tapePos++];
				tapeParamZeroLength|=tapeFile[tapePos++]<<8;
				tapeParamOneLength=tapeFile[tapePos++];
				tapeParamOneLength|=tapeFile[tapePos++]<<8;
				tapeParamPilotLength=tapeFile[tapePos++];
				tapeParamPilotLength|=tapeFile[tapePos++]<<8;
				tapeParamUsedBits=tapeFile[tapePos++];
				tapeParamDelayLength=tapeFile[tapePos++];
				tapeParamDelayLength|=tapeFile[tapePos++]<<8;
				tapeDataLength=tapeFile[tapePos++];
				tapeDataLength|=tapeFile[tapePos++]<<8;
				tapeDataLength|=tapeFile[tapePos++]<<16;
				if (tapeDataLength==1)
				{
					tapeDataBitCnt=tapeParamUsedBits;
				}
				tapeParamLastCase=8;
				return 1;
			case 0x12:
				printf("Pure Tone\n");
				tapeParamPilotPulseLength=tapeFile[tapePos++];
				tapeParamPilotPulseLength|=tapeFile[tapePos++]<<8;
				tapeParamPilotLength=tapeFile[tapePos++];
				tapeParamPilotLength|=tapeFile[tapePos++]<<8;
				tapeParamLastCase=1;
				return 1;
			case 0x13:
				printf("Pulse Sequence\n");
				tapeParamPilotLength=tapeFile[tapePos++];
				tapeParamPilotPulseLength=tapeFile[tapePos++];
				tapeParamPilotPulseLength|=tapeFile[tapePos++]<<8;
				tapeParamLastCase=9;
				return 9;
			case 0x14:
				printf("Pure Data\n");
				tapeParamZeroLength=tapeFile[tapePos++];
				tapeParamZeroLength|=tapeFile[tapePos++]<<8;
				tapeParamOneLength=tapeFile[tapePos++];
				tapeParamOneLength|=tapeFile[tapePos++]<<8;
				tapeParamUsedBits=tapeFile[tapePos++];
				tapeParamDelayLength=tapeFile[tapePos++];
				tapeParamDelayLength|=tapeFile[tapePos++]<<8;
				tapeDataLength=tapeFile[tapePos++];
				tapeDataLength|=tapeFile[tapePos++]<<8;
				tapeDataLength|=tapeFile[tapePos++]<<16;
				if (tapeDataLength==1)
				{
					tapeDataBitCnt=tapeParamUsedBits;
				}
				tapeParamLastCase=8;
				return 4;
			case 0x20:
				printf("Silence/Stop The Tape\n");
				tapeParamDelayLength=tapeFile[tapePos++];
				tapeParamDelayLength|=tapeFile[tapePos++]<<8;
				if (tapeParamDelayLength==0)
				{
					tapeLevel=0;
					tapePaused=1;
					return 0;
				}
				tapeParamLastCase=8;
				return 7;
			case 0x21:
				printf("Group Start\n");
				tapePos+=tapeFile[tapePos]+1;
				break;
			case 0x22:
				printf("Group End\n");
				break;
			case 0x24:
				printf("Loop Start\n");
				tapeParamLoopCounter=tapeFile[tapePos++];
				tapeParamLoopCounter|=tapeFile[tapePos++]<<8;
				tapeParamLoopMarker=tapePos;
				break;
			case 0x25:
				printf("Loop End\n");
				tapeParamLoopCounter--;
				if (tapeParamLoopCounter!=0)
				{
					tapePos=tapeParamLoopMarker;
				}
				break;
			case 0x2A:
				printf("Stop Tape (48K)\n");
				blockLength=tapeFile[tapePos++];
				blockLength|=tapeFile[tapePos++]<<8;
				blockLength|=tapeFile[tapePos++]<<16;
				blockLength|=tapeFile[tapePos++]<<24;
				tapePos+=blockLength;
				PauseTape();
				break;
			case 0x30:
				printf("Text Description\n");
				tapePos+=tapeFile[tapePos]+1;
				break;
			case 0x32:
				printf("Archive Info\n");
				blockLength=tapeFile[tapePos++];
				blockLength|=tapeFile[tapePos++]<<8;
				tapePos+=blockLength;
				break;
			case 0x35:
				printf("Custom Info\n");
				tapePos+=16;
				blockLength=tapeFile[tapePos++];
				blockLength|=tapeFile[tapePos++]<<8;
				blockLength|=tapeFile[tapePos++]<<16;
				blockLength|=tapeFile[tapePos++]<<24;
				tapePos+=blockLength;
				break;
			default:
				printf("UnhandledID %d\n",blockId);
				break;
		}
	}
}

int GetNextBlockParamsTape()
{
	if (tapeFormat==0)
	{
		return 99;
	}
	if (tapeFormat==1)
	{
		if (tapePos>=tapeLength)
		{
			tapePos=0;
		}
		tapeDataLength=tapeFile[tapePos++];
		tapeDataLength|=tapeFile[tapePos++]<<8;
		tapeParamDelayLength=(uint32_t)(1*3494.4f);
		tapeParamPilotPulseLength=2168;
		if (tapeFile[tapePos]<0x80)
		{
			// Header Pilot Pulse
			tapeParamPilotLength=8063;
		}
		else
		{
			// data Pilot Pulse
			tapeParamPilotLength=3223;
		}
		tapeParamSync1Length=667;
		tapeParamSync2Length=735;
		tapeParamOneLength=1710;
		tapeParamZeroLength=855;
		tapeParamLastCase=8;
		tapeParamUsedBits=8;
		return 1;
	}
	if (tapeFormat==2)
	{
		return GetNextBlockTZX(); 
	}
	return 99;
}

void UpdateTape(uint8_t cycles)
{
	int redo=1;
	if (!tapeFile || tapePaused || !cycles)
		return;
	
	while (cycles||redo)
	{
		cycles--;
		redo=0;
		switch (tapePhase)
		{
			case 0:								// New block
				tapePhase = GetNextBlockParamsTape();

				if (tapePhase!=1)
				{
					redo=1;
					break;
				}

				tapePilotLength=tapeParamPilotLength;
				tapePilotPulseLength=tapeParamPilotPulseLength;

				// Fall through - intentional
			case 1:								// Pilot pulse
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapePilotLength--;
					if (tapePilotLength==0)
					{
						tapePhase=2;
						tapePilotPulseLength=tapeParamSync1Length;
						if (tapeParamLastCase==1)
						{
							tapePhase=0;
						}
					}
				}
				break;
			case 2:								// Sync 1 pulse
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapePhase=3;
					tapePilotPulseLength=tapeParamSync2Length;
				}
				break;
			case 3:								// Sync 2 pulse
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapeDataBit=0x80;
					tapeDataBitCnt=8;
					if (tapeDataLength==1)
					{
						tapeDataBitCnt=tapeParamUsedBits;
					}
					tapePhase=4;
				}
				break;
			case 4:								// Data
				// For each byte in tapDataLength
				// We get a bit, set the correct pulse length
				// rinse and repeat
				if (tapeFile[tapePos]&tapeDataBit)
				{
					tapePilotPulseLength=tapeParamOneLength;
				}
				else
				{
					tapePilotPulseLength=tapeParamZeroLength;
				}
				tapePhase=5;
				// Fallthrough
			case 5:
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapePhase=6;
				}
				break;
			case 6:
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapeLevel=~tapeLevel;
					tapeDataBit>>=1;
					tapeDataBitCnt--;
					tapePhase=4;
					if (tapeDataBitCnt==0)
					{
						tapeDataBit=0x80;
						tapeDataBitCnt=8;
						tapeDataLength--;
						tapePos++;
						if (tapeDataLength==1)
						{
							tapeDataBitCnt=tapeParamUsedBits;
						}
						else
						{
							if (tapeDataLength==0)
							{
								if (tapeParamDelayLength==0)
								{
									tapePhase=0;
								}
								else
								{
									tapePilotPulseLength=(uint32_t)(1*3494.4f);
									tapePhase=7;
								}
							}
						}
					}
				}
				break;
			case 7:
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapePilotPulseLength=(uint32_t)((tapeParamDelayLength-1)*3494.4f);
					tapePhase=8;
					if (tapePilotPulseLength==0)
					{
						tapePhase=0;
					}
					tapeLevel=0;					// After a silence block, level must be 0
				}
				break;
			case 8:
				tapeTCounter++;
				if (tapeTCounter>=tapePilotPulseLength)
				{
					tapeTCounter-=tapePilotPulseLength;
					tapePhase=0;
				}
				break;
			case 9:								// Pilot pulse sequence special case
				tapeTCounter++;
				if (tapeTCounter>=tapeParamPilotPulseLength)
				{
					tapeTCounter-=tapeParamPilotPulseLength;
					tapeLevel=~tapeLevel;
					tapeParamPilotLength--;
					if (tapeParamPilotLength==0)
					{
						tapePhase=0;
					}
					else
					{
						tapeParamPilotPulseLength=tapeFile[tapePos++];
						tapeParamPilotPulseLength|=tapeFile[tapePos++]<<8;
					}
				}
				break;

			case 99:
				tapeTCounter+=4096;
				if (tapeTCounter>=(TSTATES_PER_ROW*ROWS_PER_VBLANK*4096)/((44100)/50))
				{
					tapeTCounter-=(TSTATES_PER_ROW*ROWS_PER_VBLANK*4096)/((44100)/50);
					tapePos++;
					if (tapePos>=tapeLength)
					{
						tapePos=0;
					}

					tapeLevel=tapeFile[tapePos]<0x80?0x00:0xFF;
				}
				break;
		}
	}
}

uint8_t GetTapeLevel()
{
	return tapeLevel;
}

