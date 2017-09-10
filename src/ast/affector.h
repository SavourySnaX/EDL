#pragma once

class CAffector : public CExpression
{
public:
	AffectorList affectors;
	CExpression& expr;
	YYLTYPE exprLoc;

	CAffector(AffectorList& affectors, CExpression& expr, YYLTYPE exprLoc) : affectors(affectors), expr(expr), exprLoc(exprLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
