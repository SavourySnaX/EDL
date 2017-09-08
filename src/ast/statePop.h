#pragma once

class CStatePop : public CStatement
{
public:
	CIdentifier& ident;

	CStatePop(CIdentifier& ident) : ident(ident) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
