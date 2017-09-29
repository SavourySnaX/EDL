#pragma once

class CIfStatement : public CStatement
{
private:
	CBlock empty;
public:
	CExpression& expr;
	CBlock& block;
	CBlock& elseBlock;

	CIfStatement(CExpression& expr, CBlock& block) : expr(expr), block(block), elseBlock(empty) { }
	CIfStatement(CExpression& expr, CBlock& block, CBlock& elseBlock) : expr(expr), block(block), elseBlock(elseBlock) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
