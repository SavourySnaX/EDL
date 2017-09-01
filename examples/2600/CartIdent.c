
//
// stella auto detect
//
#include <string.h>
#include <stdint.h>

int searchForBytes(const uint8_t* image, uint32_t imagesize, const uint8_t* signature, uint32_t sigsize, uint32_t minhits)
{
	uint32_t i,j;
	uint32_t count = 0;
	for(i = 0; i < imagesize - sigsize; ++i)
	{
		uint32_t matches = 0;
		for(j = 0; j < sigsize; ++j)
		{
			if(image[i+j] == signature[j])
				++matches;
			else
				break;
		}
		if(matches == sigsize)
		{
			++count;
			i += sigsize;  // skip past this signature 'window' entirely
		}
		if(count >= minhits)
			break;
	}

	return (count >= minhits)?1:0;
}

int isProbablyCV(const uint8_t* image, uint32_t size)
{
	// CV RAM access occurs at addresses $f3ff and $f400
	// These signatures are attributed to the MESS project
	uint8_t signature[2][3] = {
		{ 0x9D, 0xFF, 0xF3 },  // STA $F3FF.X
		{ 0x99, 0x00, 0xF4 }   // STA $F400.Y
	};
	if(searchForBytes(image, size, signature[0], 3, 1))
		return 1;
	else
		return searchForBytes(image, size, signature[1], 3, 1);
}

int isProbably4KSC(const uint8_t* image, uint32_t size)
{
	// We check if the first 256 bytes are identical *and* if there's
	// an "SC" signature for one of our larger SC types at 1FFA.

	uint32_t i;
	uint8_t first = image[0];
	for(i = 1; i < 256; ++i)
	{
		if(image[i] != first)
		{
			return 0;
		}
	}

	if((image[size-6]=='S') && (image[size-5]=='C'))
		return 1;

	return 0;
}

int isProbablySC(const uint8_t* image, uint32_t size)
{
	// We assume a Superchip cart contains the same bytes for its entire
	// RAM area; obviously this test will fail if it doesn't
	// The RAM area will be the first 256 bytes of each 4K bank
	uint32_t i,j;
	uint32_t banks = size / 4096;
	for(i = 0; i < banks; ++i)
	{
		uint8_t first = image[i*4096];
		for(j = 0; j < 256; ++j)
		{
			if(image[i*4096+j] != first)
				return 0;
		}
	}
	return 1;
}

int isProbablyE0(const uint8_t* image, uint32_t size)
{
	// E0 cart bankswitching is triggered by accessing addresses
	// $FE0 to $FF9 using absolute non-indexed addressing
	// To eliminate false positives (and speed up processing), we
	// search for only certain known signatures
	// Thanks to "stella@casperkitty.com" for this advice
	// These signatures are attributed to the MESS project
	uint8_t signature[8][3] = {
		{ 0x8D, 0xE0, 0x1F },  // STA $1FE0
		{ 0x8D, 0xE0, 0x5F },  // STA $5FE0
		{ 0x8D, 0xE9, 0xFF },  // STA $FFE9
		{ 0x0C, 0xE0, 0x1F },  // NOP $1FE0
		{ 0xAD, 0xE0, 0x1F },  // LDA $1FE0
		{ 0xAD, 0xE9, 0xFF },  // LDA $FFE9
		{ 0xAD, 0xED, 0xFF },  // LDA $FFED
		{ 0xAD, 0xF3, 0xBF }   // LDA $BFF3
	};
	uint32_t i;
	for(i = 0; i < 8; ++i)
		if(searchForBytes(image, size, signature[i], 3, 1))
			return 1;

	return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int isProbablyE7(const uint8_t* image, uint32_t size)
{
	// E7 cart bankswitching is triggered by accessing addresses
	// $FE0 to $FE6 using absolute non-indexed addressing
	// To eliminate false positives (and speed up processing), we
	// search for only certain known signatures
	// Thanks to "stella@casperkitty.com" for this advice
	// These signatures are attributed to the MESS project
	uint8_t signature[7][3] = {
		{ 0xAD, 0xE2, 0xFF },  // LDA $FFE2
		{ 0xAD, 0xE5, 0xFF },  // LDA $FFE5
		{ 0xAD, 0xE5, 0x1F },  // LDA $1FE5
		{ 0xAD, 0xE7, 0x1F },  // LDA $1FE7
		{ 0x0C, 0xE7, 0x1F },  // NOP $1FE7
		{ 0x8D, 0xE7, 0xFF },  // STA $FFE7
		{ 0x8D, 0xE7, 0x1F }   // STA $1FE7
	};
	uint32_t i;
	for(i = 0; i < 7; ++i)
		if(searchForBytes(image, size, signature[i], 3, 1))
			return 1;

	return 0;
}

int isProbably3E(const uint8_t* image, uint32_t size)
{
	// 3E cart bankswitching is triggered by storing the bank number
	// in address 3E using 'STA $3E', commonly followed by an
	// immediate mode LDA
	uint8_t signature[] = { 0x85, 0x3E, 0xA9, 0x00 };  // STA $3E; LDA #$00
	return searchForBytes(image, size, signature, 4, 1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int isProbably3F(const uint8_t* image, uint32_t size)
{
	// 3F cart bankswitching is triggered by storing the bank number
	// in address 3F using 'STA $3F'
	// We expect it will be present at least 2 times, since there are
	// at least two banks
	uint8_t signature[] = { 0x85, 0x3F };  // STA $3F
	return searchForBytes(image, size, signature, 2, 2);
}

int isProbablyUA(const uint8_t* image, uint32_t size)
{
	// UA cart bankswitching switches to bank 1 by accessing address 0x240
	// using 'STA $240' or 'LDA $240'
	uint8_t signature[3][3] = {
		{ 0x8D, 0x40, 0x02 },  // STA $240
		{ 0xAD, 0x40, 0x02 },  // LDA $240
		{ 0xBD, 0x1F, 0x02 }   // LDA $21F,X
	};
	uint32_t i;
	for(i = 0; i < 3; ++i)
		if(searchForBytes(image, size, signature[i], 3, 1))
			return 1;

	return 0;
}

int isProbablyFE(const uint8_t* image, uint32_t size)
{
	// FE bankswitching is very weird, but always seems to include a
	// 'JSR $xxxx'
	// These signatures are attributed to the MESS project
	uint8_t signature[4][5] = {
		{ 0x20, 0x00, 0xD0, 0xC6, 0xC5 },  // JSR $D000; DEC $C5
		{ 0x20, 0xC3, 0xF8, 0xA5, 0x82 },  // JSR $F8C3; LDA $82
		{ 0xD0, 0xFB, 0x20, 0x73, 0xFE },  // BNE $FB; JSR $FE73
		{ 0x20, 0x00, 0xF0, 0x84, 0xD6 }   // JSR $F000; STY $D6
	};
	uint32_t i;
	for(i = 0; i < 4; ++i)
		if(searchForBytes(image, size, signature[i], 5, 1))
			return 1;

	return 0;
}

int isProbably0840(const uint8_t* image, uint32_t size)
{
	// 0840 cart bankswitching is triggered by accessing addresses 0x0800
	// or 0x0840 at least twice
	uint8_t signature1[3][3] = {
		{ 0xAD, 0x00, 0x08 },  // LDA $0800
		{ 0xAD, 0x40, 0x08 },  // LDA $0840
		{ 0x2C, 0x00, 0x08 }   // BIT $0800
	};
	uint32_t i;
	for(i = 0; i < 3; ++i)
		if(searchForBytes(image, size, signature1[i], 3, 2))
			return 1;

	uint8_t signature2[2][4] = {
		{ 0x0C, 0x00, 0x08, 0x4C },  // NOP $0800; JMP ...
		{ 0x0C, 0xFF, 0x0F, 0x4C }   // NOP $0FFF; JMP ...
	};
	for(i = 0; i < 2; ++i)
		if(searchForBytes(image, size, signature2[i], 4, 2))
			return 1;

	return 0;
}

int isProbablyDPCplus(const uint8_t* image, uint32_t size)
{
	// DPC+ ARM code has 2 occurrences of the string DPC+
	uint8_t signature[] = { 'D', 'P', 'C', '+' };
	return searchForBytes(image, size, signature, 4, 2);
}

int isProbablyFA2(const uint8_t* image, uint32_t size)
{
	// This currently tests only the 32K version of FA2; the 24 and 28K
	// versions are easy, in that they're the only possibility with those
	// file sizes

	// 32K version has all zeros in 29K-32K area
	uint32_t i;
	for(i = 29*1024; i < 32*1024; ++i)
		if(image[i] != 0)
			return 0;

	return 1;
}

///

const char* CartIdentify(const uint8_t* rom,uint32_t size)
{
	if ((size%8448)==0 || size==6144)
	{
		return "AR";			// TODO may need more than one mapper if lots of different sizes
	}
	if (size<2048)
	{
		return "2K";
	}
	if (size==2048 || (size==4096 && memcmp(rom,rom+2048,2048)==0))
	{
		return isProbablyCV(rom,size) ? "CV2K" : "2K";
	}
	if (size==4096)
	{
		if (isProbablyCV(rom,size))
			return "CV4K";
		if (isProbably4KSC(rom,size))
			return "4KSC";
		return "4K";
	}
	if (size==8192)
	{
		// First check for *potential* F8
		uint8_t signature[] = { 0x8D, 0xF9, 0x1F };  // STA $1FF9
		int f8 = searchForBytes(rom, size, signature, 3, 2);

		if(isProbablySC(rom, size))
			return "8KSC";
		else if(memcmp(rom, rom + 4096, 4096) == 0)
			return "4K";
		else if(isProbablyE0(rom, size))
			return "8KE0";
		else if(isProbably3E(rom, size))
			return "8K3E";
		else if(isProbably3F(rom, size))
			return "8K3F";
		else if(isProbablyUA(rom, size))
			return "8KUA";
		else if(isProbablyFE(rom, size) && !f8)
			return "8KFE";
		else if(isProbably0840(rom, size))
			return "0840";
		else
			return "8K";
	}
	if (size>=10240 && size<=10496)
	{
		return "DPC";			// Pitfall 2
	}
	if (size==12288)
	{
		return "12K";
	}
	if (size==16384)
	{
		if(isProbablySC(rom, size))
			return "16KSC";
		else if(isProbablyE7(rom, size))
			return "16KE7";
		else if(isProbably3E(rom, size))
			return "16K3E";
		/* no known 16K 3F ROMS
		   else if(isProbably3F(rom, size))
		   type = "3F";
		   */
		else
			return "16K";
	}
	if(size == 32768)
	{
		if(isProbablySC(rom, size))
			return "32KSC";
		else if(isProbably3E(rom, size))
			return "32K3E";
		else if(isProbably3F(rom, size))
			return "32K3F";
		else if(isProbablyDPCplus(rom, size))
			return "DPC+";
		/*else if(isProbablyCTY(rom, size))
			return "CTY";*/
		else if(isProbablyFA2(rom, size))
			return "32KFA2";
		else
			return "32K";
	}
	return "";
}
////
////string Cartridge::autodetectType(const uInt8* image, uInt32 size)
////{
////  // Guess type based on size
////  const char* type = 0;
////
////  else if(size == 24*1024 || size == 28*1024)  // 24K & 28K
////  {
////    type = "FA2";
////  }
////  else if(size == 29*1024)  // 29K
////  {
////    if(isProbablyARM(image, size))
////      type = "FA2";
////    else /*if(isProbablyDPCplus(image, size))*/
////      type = "DPC+";
////  }
////  else if(size == 64*1024)  // 64K
////  {
////    if(isProbably3E(image, size))
////      type = "3E";
////    else if(isProbably3F(image, size))
////      type = "3F";
////    else if(isProbably4A50(image, size))
////      type = "4A50";
////    else if(isProbablyEF(image, size, type))
////      ; // type has been set directly in the function
////    else if(isProbablyX07(image, size))
////      type = "X07";
////    else
////      type = "F0";
////  }
////  else if(size == 128*1024)  // 128K
////  {
////    if(isProbably3E(image, size))
////      type = "3E";
////    else if(isProbablyDF(image, size, type))
////      ; // type has been set directly in the function
////    else if(isProbably3F(image, size))
////      type = "3F";
////    else if(isProbably4A50(image, size))
////      type = "4A50";
////    else if(isProbablySB(image, size))
////      type = "SB";
////    else
////      type = "MC";
////  }
////  else if(size == 256*1024)  // 256K
////  {
////    if(isProbably3E(image, size))
////      type = "3E";
////    else if(isProbablyBF(image, size, type))
////      ; // type has been set directly in the function
////    else if(isProbably3F(image, size))
////      type = "3F";
////    else /*if(isProbablySB(image, size))*/
////      type = "SB";
////  }
////  else  // what else can we do?
////  {
////    if(isProbably3E(image, size))
////      type = "3E";
////    else if(isProbably3F(image, size))
////      type = "3F";
////    else
////      type = "4K";  // Most common bankswitching type
////  }
////
////  return type;
////}
////
