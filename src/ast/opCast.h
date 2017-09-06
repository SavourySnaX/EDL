#pragma once

class CCastOperator : public CExpression
{
public:
	CExpression& lhs;
	CInteger& beg;
	CInteger& end;
	static CInteger begZero;
	YYLTYPE operatorLoc;

	CCastOperator(CExpression& lhs, CInteger& end, YYLTYPE operatorLoc) : lhs(lhs), beg(begZero), end(end), operatorLoc(operatorLoc) { }
	CCastOperator(CExpression& lhs, CInteger& beg, CInteger& end, YYLTYPE operatorLoc) : lhs(lhs), beg(beg), end(end), operatorLoc(operatorLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);

	virtual bool IsLeaf() const
	{
		return lhs.IsLeaf();
	}
};
