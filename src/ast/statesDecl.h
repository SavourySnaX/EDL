#pragma once

class CStatesDeclaration : public CStatement 
{
private:
	StateList states;
	int ComputeBaseIdx(StateIdentList& list, int index, int &total) const;
public:
	std::string label;
	llvm::BasicBlock* exitState;
	llvm::Value* optocurState;
	CBlock& block;
	YYLTYPE statementLoc;

	CStatesDeclaration(StateList& states, CBlock& block, YYLTYPE *statementLoc) : states(states), block(block), statementLoc(*statementLoc) { }

	int GetNumStates() const;
	bool FindBaseIdx(CStatesDeclaration* find,int& idx) const;
	int ComputeBaseIdx(StateIdentList& list) const { int dontCare = 0; return ComputeBaseIdx(list, 1, dontCare); }
	int ComputeBaseIdx(StateIdentList& list, int &total) const { return ComputeBaseIdx(list, 1, total); }
	CStateDeclaration* getStateDeclaration(const CIdentifier& id) const
	{
		for (const auto& state : states)
		{
			if (state->id.name == id.name)
			{
				return state;
			}
		}
		return nullptr; 
	}
	int getStateDeclarationIndex(const CIdentifier& id) const 
	{ 
		for (int a = 0; a < states.size(); a++) 
		{ 
			if (states[a]->id.name == id.name) 
				return a; 
		} 
		return -1; 
	}

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
