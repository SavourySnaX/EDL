#pragma once

class CExchange : public CStatement
{
public:
	CIdentifier&	lhs;
	CIdentifier&	rhs;
	YYLTYPE operatorLoc;

	CExchange(CIdentifier& lhs, CIdentifier& rhs, YYLTYPE *operatorLoc) : lhs(lhs), rhs(rhs), operatorLoc(*operatorLoc) { }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};
