#pragma once

class CStateJump : public CStatement
{
public:
	StateIdentList stateIdents;

	CStateJump(StateIdentList& stateIdents) : stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
