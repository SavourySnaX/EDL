#pragma once

class CExternDecl : public CStatement
{
public:
	ExternParamsList returns;
	CIdentifier& name;
	ExternParamsList params;
	YYLTYPE declarationLoc;

	CExternDecl(ExternParamsList& returns, CIdentifier& name, ExternParamsList& params, YYLTYPE declarationLoc) : returns(returns), name(name), params(params), declarationLoc(declarationLoc) { }
	CExternDecl(CIdentifier& name, ExternParamsList& params, YYLTYPE declarationLoc) : name(name), params(params), declarationLoc(declarationLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
