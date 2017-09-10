#pragma once

class CFunctionDecl : public CStatement
{
public:
	bool internal;
	NamedParamsList returns;
	CIdentifier& name;
	NamedParamsList params;
	CBlock& block;
	YYLTYPE functionLoc;

	CFunctionDecl(bool internal, NamedParamsList& returns, CIdentifier& name, NamedParamsList& params, CBlock &block, YYLTYPE* functionLoc) : internal(internal), returns(returns), name(name), params(params), block(block), functionLoc(*functionLoc) { }
	CFunctionDecl(bool internal, CIdentifier& name, NamedParamsList& params, CBlock &block, YYLTYPE* functionLoc) : internal(internal), name(name), params(params), block(block), functionLoc(*functionLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
