#include "yyltype.h"
#include "ast.h"

#include <llvm/IR/Constants.h>

CInteger::CInteger(std::string value)
{
	unsigned char radix;
	const char* data;
	unsigned numBits;
	if (value[0] == '%')
	{
		numBits = value.length() - 1;
		data = &value.c_str()[1];
		radix = 2;
	}
	else
	{
		if (value[0] == '$')
		{
			numBits = 4 * (value.length() - 1);
			data = &value.c_str()[1];
			radix = 16;
		}
		else
		{
			numBits = 4 * (value.length());	/* over allocation for now */
			data = &value.c_str()[0];
			radix = 10;
		}
	}

	integer = llvm::APInt(numBits, data, radix);
	if (radix == 10)
	{
		if (integer.getActiveBits())						// this shrinks the value to the correct number of bits - fixes over allocation for decimal numbers
		{
			if (integer.getActiveBits() != numBits)
			{
				integer = integer.trunc(integer.getActiveBits());		// only performed on decimal numbers (otherwise we loose important leading zeros)
			}
		}
		else
		{
			integer = integer.trunc(1);
		}
	}
}

llvm::Value* CInteger::codeGen(CodeGenContext& context)
{
	return llvm::ConstantInt::get(TheContext, integer);
}

