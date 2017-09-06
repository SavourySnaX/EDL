#pragma once

class CBinaryOperator : public CExpression
{
public:
	int op;
	CExpression& lhs;
	CExpression& rhs;
	YYLTYPE operatorLoc;

	CBinaryOperator(CExpression& lhs, int op, CExpression& rhs, YYLTYPE *operatorLoc) : lhs(lhs), rhs(rhs), op(op), operatorLoc(*operatorLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
	llvm::Value* codeGen(CodeGenContext& context, llvm::Value* left, llvm::Value* right);

	virtual bool IsCarryExpression() const;

	virtual bool IsLeaf() const
	{
		return false;
	}
};
