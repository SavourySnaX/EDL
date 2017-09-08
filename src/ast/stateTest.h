#pragma once

class CStateTest : public CExpression 
{
public:
	StateIdentList stateIdents;

	CStateTest(StateIdentList& stateIdents) : stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
