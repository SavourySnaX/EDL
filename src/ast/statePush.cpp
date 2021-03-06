#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

void CStatePush::prePass(CodeGenContext& context)
{

}


llvm::Value* CStatePush::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE." + stateIdents[0]->name;

	if (context.states().find(stateLabel) == context.states().end())
	{
		return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, stateIdents[0]->nameLoc, "Unknown handler, can't look up state reference");
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates = topState.decl->GetNumStates();
	int jumpIndex;
	if (!topState.decl->FindStateIdx(stateIdents, jumpIndex))
	{
		return context.gContext.ReportUndefinedStateError(stateIdents);
	}

	llvm::Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	llvm::APInt overSized(4 * numStates.length(), numStates, 10);
	unsigned bitsNeeded = overSized.getActiveBits();

	llvm::Value* index = new llvm::LoadInst(topState.stateStackIndex->getType()->getPointerElementType(), topState.stateStackIndex, "stackIndex", false, context.currentBlock());
	std::vector<llvm::Value*> indices;
	llvm::ConstantInt* const_intn = context.getConstantZero(bitsNeeded);
	indices.push_back(const_intn);
	indices.push_back(index);
	llvm::Value* ref = llvm::GetElementPtrInst::Create(nullptr, topState.stateStackNext, indices, "stackPos", context.currentBlock());

	llvm::Value* curNext = new llvm::LoadInst(topState.nextState->getType()->getPointerElementType(), topState.nextState, "currentNext", false, context.currentBlock());

	new llvm::StoreInst(curNext, ref, false, context.currentBlock());			// Save current next point to stack

	llvm::Value* inc = llvm::BinaryOperator::Create(llvm::Instruction::Add, index, context.getConstantInt(llvm::APInt(MAX_SUPPORTED_STACK_BITS, 1)), "incrementIndex", context.currentBlock());
	new llvm::StoreInst(inc, topState.stateStackIndex, false, context.currentBlock());	// Save updated index

	// Set next state
	return new llvm::StoreInst(context.getConstantInt(llvm::APInt(bitsNeeded, jumpIndex)), topState.nextState, false, context.currentBlock());
}
