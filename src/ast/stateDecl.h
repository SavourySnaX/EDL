#pragma once

class CStateDeclaration : public CStatement
{
public:
	CStatesDeclaration* child;
	CIdentifier& id;
	bool autoIncrement;
	llvm::BasicBlock* entry;
	llvm::BasicBlock* exit;

	CStateDeclaration(CIdentifier& id) : id(id), autoIncrement(false), entry(nullptr), exit(nullptr), child(nullptr) { }

	int GetNumStates() const;
	bool FindBaseIdx(CStatesDeclaration* find,int& idx) const;

	virtual llvm::Value* codeGen(CodeGenContext& context, llvm::Function* parent);
};
