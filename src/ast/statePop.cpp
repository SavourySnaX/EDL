#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

void CStatePop::prePass(CodeGenContext& context)
{

}

llvm::Value* CStatePop::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE." + ident.name;

	if (context.states().find(stateLabel) == context.states().end())
	{
		return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, ident.nameLoc, "Unknown handler, can't look up state reference");
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates = topState.decl->GetNumStates();
	int totalInBlock;

	llvm::Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	llvm::APInt overSized(4 * numStates.length(), numStates, 10);
	unsigned bitsNeeded = overSized.getActiveBits();

	// We need to pop from our stack and put next back
	llvm::Value* index = new llvm::LoadInst(topState.stateStackIndex->getType()->getPointerElementType(), topState.stateStackIndex, "stackIndex", false, context.currentBlock());
	llvm::Value* dec = llvm::BinaryOperator::Create(llvm::Instruction::Sub, index, context.getConstantInt(llvm::APInt(MAX_SUPPORTED_STACK_BITS, 1)), "decrementIndex", context.currentBlock());
	new llvm::StoreInst(dec, topState.stateStackIndex, false, context.currentBlock());	// store new stack index

	std::vector<llvm::Value*> indices;
	llvm::ConstantInt* const_intn = context.getConstantZero(bitsNeeded);
	indices.push_back(const_intn);
	indices.push_back(dec);
	llvm::Value* ref = llvm::GetElementPtrInst::Create(nullptr, topState.stateStackNext, indices, "stackPos", context.currentBlock());

	llvm::Value* oldNext = new llvm::LoadInst(ref->getType()->getPointerElementType(), ref, "oldNext", false, context.currentBlock());	// retrieve old next value

	return new llvm::StoreInst(oldNext, topState.nextState, false, context.currentBlock());	// restore next state to old value
}
