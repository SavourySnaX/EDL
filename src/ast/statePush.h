#pragma once

class CStatePush : public CStatement
{
public:
	StateIdentList stateIdents;

	CStatePush(StateIdentList& stateIdents) : stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
