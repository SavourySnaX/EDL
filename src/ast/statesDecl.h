#pragma once

class CStatesDeclaration : public CStatement 
{
private:
	StateList states;
	bool FindStateIdxAndLength(StateIdentList& list, int listOffs, int& idx, int &total) const;
public:
	std::string label;
	llvm::BasicBlock* exitState;
	llvm::Value* optocurState;
	CBlock& block;
	YYLTYPE statementLoc;

	CStatesDeclaration(StateList& states, CBlock& block, YYLTYPE *statementLoc) : states(states), block(block), statementLoc(*statementLoc) { }

	int GetNumStates() const;
	bool FindStateIdx(CStatesDeclaration* find,int& idx) const;
	bool FindStateIdx(StateIdentList& list, int& idx) const 
	{
		int dontCare = 0; 
		idx = 0;
		if (list.size() == 1)
		{
			return true;
		}

		return FindStateIdxAndLength(list, 1, idx, dontCare); 
	}
	bool FindStateIdxAndLength(StateIdentList& list, int& idx, int &length) const 
	{
		idx = 0;
		length = 0;
		if (list.size() == 1)
		{
			return true;
		}
		return FindStateIdxAndLength(list, 1, idx, length); 
	}

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
