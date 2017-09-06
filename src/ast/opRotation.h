#pragma once

class CRotationOperator : public CExpression
{
public:
	CExpression& value;
	CIdentifier& bitsOut;
	CExpression& bitsIn;
	CExpression& rotAmount;
	int direction;

	CRotationOperator(int direction, CExpression& value, CIdentifier& bitsOut, CExpression& bitsIn, CExpression& rotAmount) :
		direction(direction), value(value), bitsOut(bitsOut), bitsIn(bitsIn), rotAmount(rotAmount) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
