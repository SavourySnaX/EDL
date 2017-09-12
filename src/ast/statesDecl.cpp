#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

int CStatesDeclaration::GetNumStates() const
{
	int totalStates = 0;

	for (const auto& state : states)
	{
		totalStates += state->GetNumStates();
	}

	return totalStates;
}

bool CStatesDeclaration::FindStateIdx(CStatesDeclaration* find,int& idx) const
{
	for (const auto& state : states)
	{
		if (state->FindStateIdx(find, idx))
		{
			return true;
		}
	}

	return false;
}

bool CStatesDeclaration::FindStateIdxAndLength(StateIdentList& list, int listOffs,int &idx, int &total) const
{
	for (const auto& state : states)
	{
		if (state->id.name == list[listOffs]->name)
		{
			if (listOffs == list.size() - 1)
			{
				total = state->GetNumStates();
				return true;
			}

			if (state->hasSubStates())
			{
				if (state->getSubStates()->FindStateIdxAndLength(list, listOffs + 1, idx, total))
				{
					return true;
				}
			}
			else
			{
				idx++;
			}
		}
		else
		{
			// Skip the remaining states in this substate
			idx += state->GetNumStates();
		}
	}
	return false;
}

void CStatesDeclaration::prePass(CodeGenContext& context)
{
	int a;

	context.pushState(this);

	block.prePass(context);

	context.popState();

	if (context.currentIdent() != nullptr)
	{
		int stateIndex = context.currentState()->getStateDeclarationIndex(*context.currentIdent());

		if (stateIndex != -1)
		{
			context.currentState()->states[stateIndex]->AssignSubStates(this);
		}
	}
}

llvm::Value* CStatesDeclaration::codeGen(CodeGenContext& context)
{
	bool TopMostState = false;

	if (context.isStateEmpty())
	{
		if (context.parentHandler == nullptr)
		{
			PrintErrorFromLocation(statementLoc, "It is illegal to declare STATES outside of a handler");
			context.FlagError();
			return nullptr;
		}

		TopMostState = true;
	}

	int totalStates;
	std::string numStates;
	llvm::APInt overSized;
	unsigned bitsNeeded;
	int baseStateIdx=0;

	llvm::Value *curState;
	llvm::Value *nxtState;
	std::string stateLabel = "STATE" + context.stateLabelStack;

	llvm::Value* switchValue = nullptr;
	if (TopMostState)
	{
		totalStates = GetNumStates();
		llvm::Twine numStatesTwine(totalStates);
		numStates = numStatesTwine.str();
		overSized = llvm::APInt(4 * numStates.length(), numStates, 10);
		bitsNeeded = overSized.getActiveBits();

		std::string curStateLbl = "CUR" + context.stateLabelStack;
		std::string nxtStateLbl = "NEXT" + context.stateLabelStack;
		std::string idxStateLbl = "IDX" + context.stateLabelStack;
		std::string stkStateLbl = "STACK" + context.stateLabelStack;

		llvm::GlobalVariable* gcurState = context.makeGlobal(context.getIntType(bitsNeeded), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.getSymbolPrefix() + curStateLbl);
		llvm::GlobalVariable* gnxtState = context.makeGlobal(context.getIntType(bitsNeeded), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.getSymbolPrefix() + nxtStateLbl);

		curState = gcurState;
		nxtState = gnxtState;

		llvm::ArrayType* ArrayTy_0 = llvm::ArrayType::get(context.getIntType(bitsNeeded), MAX_SUPPORTED_STACK_DEPTH);
		llvm::GlobalVariable* stkState = context.makeGlobal(ArrayTy_0, false, llvm::GlobalValue::PrivateLinkage, nullptr, context.getSymbolPrefix() + stkStateLbl);
		llvm::GlobalVariable *stkStateIdx = context.makeGlobal(context.getIntType(MAX_SUPPORTED_STACK_BITS), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.getSymbolPrefix() + idxStateLbl);

		StateVariable newStateVar;
		newStateVar.currentState = curState;
		newStateVar.nextState = nxtState;
		newStateVar.stateStackNext = stkState;
		newStateVar.stateStackIndex = stkStateIdx;
		newStateVar.decl = this;

		context.states()[stateLabel] = newStateVar;
		context.statesAlt()[this] = newStateVar;

		// Constant Definitions
		llvm::ConstantInt* const_int32_n = context.getConstantZero(bitsNeeded);
		llvm::ConstantInt* const_int4_n = context.getConstantInt(llvm::APInt(MAX_SUPPORTED_STACK_BITS, 0, false));
		llvm::ConstantAggregateZero* const_array_n = llvm::ConstantAggregateZero::get(ArrayTy_0);

		// Global Variable Definitions
		gcurState->setInitializer(const_int32_n);
		gnxtState->setInitializer(const_int32_n);
		stkStateIdx->setInitializer(const_int4_n);
		stkState->setInitializer(const_array_n);
	}
	else
	{
		StateVariable topState = context.states()[stateLabel];

		curState = topState.currentState;
		nxtState = topState.nextState;

		totalStates = topState.decl->GetNumStates();
		llvm::Twine numStatesTwine(totalStates);
		numStates = numStatesTwine.str();
		overSized = llvm::APInt(4 * numStates.length(), numStates, 10);
		bitsNeeded = overSized.getActiveBits();

		if (!topState.decl->FindStateIdx(this, baseStateIdx))
		{
			assert(0 && "internal error, this should not occur - - might need to be an error -- undefined state");
		}

		switchValue = topState.decl->optocurState;
	}
	llvm::BasicBlock* bb = context.currentBlock();		// cache old basic block

	std::map<std::string, BitVariable> tmp = context.locals();

	// Setup exit from switch statement
	exitState = context.makeBasicBlock("switchTerm", bb->getParent());
	context.setBlock(exitState);

	// Step 1, load next state into current state
	if (TopMostState)
	{
		llvm::LoadInst* getNextState = new llvm::LoadInst(nxtState, "", false, bb);
		llvm::StoreInst* storeState = new llvm::StoreInst(getNextState, curState, false, bb);

		optocurState = new llvm::LoadInst(curState, "", false, bb);
		switchValue = optocurState;
	}
	// Step 2, generate switch statement
	llvm::SwitchInst* void_6 = llvm::SwitchInst::Create(switchValue, exitState, totalStates, bb);

	// Step 3, build labels and initialiser for next state on entry
	bool lastStateAutoIncrement = true;
	int startOfAutoIncrementIdx = baseStateIdx;
	int startIdx = baseStateIdx;
	for (int a = 0; a < states.size(); a++)
	{
		// Build Labels
		int total = states[a]->GetNumStates();

		states[a]->codeGen(context, bb->getParent());
		for (int b = 0; b < total; b++)
		{
			//
			llvm::ConstantInt* tt = context.getConstantInt(llvm::APInt(bitsNeeded, startIdx + b, false));
			void_6->addCase(tt, states[a]->entry);
		}
		if (!lastStateAutoIncrement)
		{
			startOfAutoIncrementIdx = startIdx;
		}
		startIdx += total;

		// Compute next state and store for future
		if (!states[a]->hasSubStates())
		{
			llvm::ConstantInt* nextState;
			if (states[a]->autoIncrement)
			{
				nextState = context.getConstantInt(llvm::APInt(bitsNeeded, a == states.size() - 1 ? baseStateIdx : startIdx, false));
			}
			else
			{
				nextState = context.getConstantInt(llvm::APInt(bitsNeeded, startOfAutoIncrementIdx, false));
			}
			llvm::StoreInst* newState = new llvm::StoreInst(nextState, nxtState, false, states[a]->entry);
		}

		lastStateAutoIncrement = states[a]->autoIncrement;
	}

	// Step 4, run code blocks to generate states
	context.pushState(this);
	block.codeGen(context);
	context.popState();

	// Finally we need to terminate the final blocks
	for (const auto& state : states)
	{
		llvm::BranchInst::Create(state->exit, state->entry);
		llvm::BranchInst::Create(exitState, state->exit);			// this terminates the final blocks from our states
	}

	return nullptr;
}
