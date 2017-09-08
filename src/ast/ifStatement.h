#pragma once

class CIfStatement : public CStatement
{
public:
	CExpression& expr;
	CBlock& block;

	CIfStatement(CExpression& expr, CBlock& block) : expr(expr), block(block) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
