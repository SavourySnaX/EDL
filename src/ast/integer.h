#pragma once

#include "nodes.h"

class CodeGenContext;

class CInteger : public CExpression
{
private:
	llvm::APInt	integer;
	YYLTYPE integerLoc;
public:
	CInteger(std::string value);
	CInteger(std::string value, YYLTYPE *_integerLoc) : CInteger(value)
	{
		integerLoc = *_integerLoc;
	}

	void Decrement()
	{
		integer-=1;
	}

	const llvm::APInt& getAPInt() const { return integer; }
	const YYLTYPE& getSourceLocation() const { return integerLoc; }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};
