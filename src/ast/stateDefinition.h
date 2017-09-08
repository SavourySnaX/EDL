#pragma once

class CStateDefinition : public CStatement
{
public:
	const CIdentifier& id;
	CBlock& block;

	CStateDefinition(const CIdentifier& id, CBlock& block) : id(id), block(block) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
