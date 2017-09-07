#pragma once

class CStateDeclaration : public CStatement
{
private:
	CStatesDeclaration* child;
public:
	CIdentifier& id;
	bool autoIncrement;
	llvm::BasicBlock* entry;
	llvm::BasicBlock* exit;

	CStateDeclaration(CIdentifier& id) : id(id), autoIncrement(false), entry(nullptr), exit(nullptr), child(nullptr) { }

	virtual llvm::Value* codeGen(CodeGenContext& context, llvm::Function* parent);
};
