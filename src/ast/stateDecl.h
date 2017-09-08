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

	int GetNumStates() const;
	bool FindStateIdx(CStatesDeclaration* find,int& idx) const;
	bool hasSubStates() const { return child != nullptr; }
	void AssignSubStates(CStatesDeclaration* subStates) { child = subStates; }
	const CStatesDeclaration* const getSubStates() const { return child; }

	virtual llvm::Value* codeGen(CodeGenContext& context, llvm::Function* parent);
};
