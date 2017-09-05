#pragma once

class CInteger : public CExpression
{
public:
	llvm::APInt	integer;
	YYLTYPE integerLoc;
	CInteger(std::string value);
	CInteger(std::string value, YYLTYPE *_integerLoc) : CInteger(value)
	{
		integerLoc = *_integerLoc;
	}

	void Decrement()
	{
		integer--;
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);
};
