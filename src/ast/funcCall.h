#pragma once

class CFuncCall : public CExpression
{
public:
	CIdentifier& name;
	ParamsList& params;
	YYLTYPE callLoc;

	CFuncCall(CIdentifier& name, ParamsList& params, YYLTYPE* callLoc) : name(name), params(params), callLoc(*callLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
