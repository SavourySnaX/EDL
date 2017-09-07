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

int CStatesDeclaration::GetNumStates(CStatesDeclaration* state) const
{
	int a;
	int totalStates = 0;

	if (state == nullptr)
		return 1;

	for (a = 0; a < state->states.size(); a++)
	{
		CStatesDeclaration* downState = state->children[a];

		if (downState == nullptr)
		{
			totalStates++;
		}
		else
		{
			totalStates += GetNumStates(downState);
		}
	}

	return totalStates;
}

int CStatesDeclaration::ComputeBaseIdx(CStatesDeclaration* cur, CStatesDeclaration* find) const
{
	int a;
	int totalStates = 0;
	static bool found = false;

	if (cur == nullptr)
	{
		return 0;
	}

	found = false;

	for (a = 0; a < cur->states.size(); a++)
	{
		CStatesDeclaration* downState = cur->children[a];

		if (downState == find)
		{
			found = true;
			return totalStates;
		}

		if (downState == nullptr)
		{
			totalStates++;
		}
		else
		{
			totalStates += ComputeBaseIdx(downState, find);
			if (found)
			{
				return totalStates;
			}
		}
	}

	return totalStates;
}

int CStatesDeclaration::ComputeBaseIdx(CStatesDeclaration* cur, StateIdentList& list, int index, int &total) const
{
	int a;
	int totalStates = 0;
	static bool found = false;

	total = 0;
	if (list.size() == 1)
		return 0;

	for (a = 0; a < cur->states.size(); a++)
	{
		if (index && (cur->states[a]->id.name == list[index]->name))
		{
			CStatesDeclaration* downState = cur->children[a];

			if (index == list.size() - 1)
			{
				found = true;
				if (downState == nullptr)
				{
					total = 1;
				}
				else
				{
					total = ComputeBaseIdx(downState, list, 0, total);
				}
				return totalStates;
			}

			if (downState == nullptr)
			{
				totalStates++;
			}
			else
			{
				int numStates = ComputeBaseIdx(downState, list, index + 1, total);
				totalStates += numStates;
				if (found)
				{
					return totalStates;
				}
			}
		}
		else
		{
			CStatesDeclaration* downState = cur->children[a];

			if (downState == nullptr)
			{
				totalStates++;
			}
			else
			{
				totalStates += ComputeBaseIdx(downState, list, 0, total);
			}

		}
	}

	if (!index)
	{
		return totalStates;
	}
	return -1;
}

void CStatesDeclaration::prePass(CodeGenContext& context)
{
	int a;

	for (a = 0; a < states.size(); a++)
	{
		children.push_back(nullptr);
	}

	context.pushState(this);

	block.prePass(context);

	context.popState();

	if (context.currentIdent() != nullptr)
	{
		int stateIndex = context.currentState()->getStateDeclarationIndex(*context.currentIdent());

		if (stateIndex != -1)
		{
			context.currentState()->children[stateIndex] = this;
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
			context.errorFlagged = true;
			return nullptr;
		}

		TopMostState = true;
	}

	int totalStates;
	std::string numStates;
	llvm::APInt overSized;
	unsigned bitsNeeded;
	int baseStateIdx;

	llvm::Value *curState;
	llvm::Value *nxtState;
	std::string stateLabel = "STATE" + context.stateLabelStack;

	llvm::Value* int32_5 = nullptr;
	if (TopMostState)
	{
		totalStates = GetNumStates(this);
		llvm::Twine numStatesTwine(totalStates);
		numStates = numStatesTwine.str();
		overSized = llvm::APInt(4 * numStates.length(), numStates, 10);
		bitsNeeded = overSized.getActiveBits();
		baseStateIdx = 0;

		std::string curStateLbl = "CUR" + context.stateLabelStack;
		std::string nxtStateLbl = "NEXT" + context.stateLabelStack;
		std::string idxStateLbl = "IDX" + context.stateLabelStack;
		std::string stkStateLbl = "STACK" + context.stateLabelStack;

		llvm::GlobalVariable* gcurState = new llvm::GlobalVariable(*context.module, llvm::Type::getIntNTy(TheContext, bitsNeeded), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.symbolPrepend + curStateLbl);
		llvm::GlobalVariable* gnxtState = new llvm::GlobalVariable(*context.module, llvm::Type::getIntNTy(TheContext, bitsNeeded), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.symbolPrepend + nxtStateLbl);

		curState = gcurState;
		nxtState = gnxtState;

		llvm::ArrayType* ArrayTy_0 = llvm::ArrayType::get(llvm::IntegerType::get(TheContext, bitsNeeded), MAX_SUPPORTED_STACK_DEPTH);
		llvm::GlobalVariable* stkState = new llvm::GlobalVariable(*context.module, ArrayTy_0, false, llvm::GlobalValue::PrivateLinkage, nullptr, context.symbolPrepend + stkStateLbl);
		llvm::GlobalVariable *stkStateIdx = new llvm::GlobalVariable(*context.module, llvm::Type::getIntNTy(TheContext, MAX_SUPPORTED_STACK_BITS), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.symbolPrepend + idxStateLbl);

		StateVariable newStateVar;
		newStateVar.currentState = curState;
		newStateVar.nextState = nxtState;
		newStateVar.stateStackNext = stkState;
		newStateVar.stateStackIndex = stkStateIdx;
		newStateVar.decl = this;

		context.states()[stateLabel] = newStateVar;
		context.statesAlt()[this] = newStateVar;

		// Constant Definitions
		llvm::ConstantInt* const_int32_n = llvm::ConstantInt::get(TheContext, llvm::APInt(bitsNeeded, 0, false));
		llvm::ConstantInt* const_int4_n = llvm::ConstantInt::get(TheContext, llvm::APInt(MAX_SUPPORTED_STACK_BITS, 0, false));
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

		totalStates = GetNumStates(topState.decl);
		llvm::Twine numStatesTwine(totalStates);
		numStates = numStatesTwine.str();
		overSized = llvm::APInt(4 * numStates.length(), numStates, 10);
		bitsNeeded = overSized.getActiveBits();

		baseStateIdx = ComputeBaseIdx(topState.decl, this);

		int32_5 = topState.decl->optocurState;
	}
	llvm::BasicBlock* bb = context.currentBlock();		// cache old basic block

	std::map<std::string, BitVariable> tmp = context.locals();

	// Setup exit from switch statement
	exitState = llvm::BasicBlock::Create(TheContext, "switchTerm", bb->getParent());
	context.setBlock(exitState);

	// Step 1, load next state into current state
	if (TopMostState)
	{
		llvm::LoadInst* getNextState = new llvm::LoadInst(nxtState, "", false, bb);
		llvm::StoreInst* storeState = new llvm::StoreInst(getNextState, curState, false, bb);
	}

	// Step 2, generate switch statement
	if (TopMostState)
	{
		optocurState = new llvm::LoadInst(curState, "", false, bb);
		int32_5 = optocurState;
	}
	llvm::SwitchInst* void_6 = llvm::SwitchInst::Create(int32_5, exitState, totalStates, bb);


	// Step 3, build labels and initialiser for next state on entry
	bool lastStateAutoIncrement = true;
	int startOfAutoIncrementIdx = baseStateIdx;
	int startIdx = baseStateIdx;
	for (int a = 0; a < states.size(); a++)
	{
		// Build Labels
		int total = GetNumStates(children[a]);

		states[a]->codeGen(context, bb->getParent());
		for (int b = 0; b < total; b++)
		{
			//
			llvm::ConstantInt* tt = llvm::ConstantInt::get(TheContext, llvm::APInt(bitsNeeded, startIdx + b, false));
			void_6->addCase(tt, states[a]->entry);
		}
		if (!lastStateAutoIncrement)
		{
			startOfAutoIncrementIdx = startIdx;
		}
		startIdx += total;

		// Compute next state and store for future
		if (states[a]->autoIncrement)
		{
			if (children[a] == nullptr)
			{
				llvm::ConstantInt* nextState = llvm::ConstantInt::get(TheContext, llvm::APInt(bitsNeeded, a == states.size() - 1 ? baseStateIdx : startIdx, false));
				llvm::StoreInst* newState = new llvm::StoreInst(nextState, nxtState, false, states[a]->entry);
			}
		}
		else
		{
			if (children[a] == nullptr)
			{
				llvm::ConstantInt* nextState = llvm::ConstantInt::get(TheContext, llvm::APInt(bitsNeeded, startOfAutoIncrementIdx, false));
				llvm::StoreInst* newState = new llvm::StoreInst(nextState, nxtState, false, states[a]->entry);
			}
		}

		lastStateAutoIncrement = states[a]->autoIncrement;
	}

	// Step 4, run code blocks to generate states
	context.pushState(this);
	block.codeGen(context);
	context.popState();

	// Finally we need to terminate the final blocks
	for (int a = 0; a < states.size(); a++)
	{
		llvm::BranchInst::Create(states[a]->exit, states[a]->entry);
		llvm::BranchInst::Create(exitState, states[a]->exit);			// this terminates the final blocks from our states
	}

	return nullptr;
}
