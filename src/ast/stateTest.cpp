#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/Twine.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away
extern llvm::Value* UndefinedStateError(StateIdentList &stateIdents, CodeGenContext &context);	// Todo refactor away

void CStateTest::prePass(CodeGenContext& context)
{
	
}

llvm::Value* CStateTest::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE."+stateIdents[0]->name;

	if (context.states().find(stateLabel)==context.states().end())
	{
		PrintErrorFromLocation(stateIdents[0]->nameLoc, "Unknown handler, can't look up state reference");
		context.FlagError();
		return nullptr;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates=topState.decl->GetNumStates();
	int totalInBlock;
	int jumpIndex;

	if (!topState.decl->FindStateIdxAndLength(stateIdents, jumpIndex, totalInBlock))
	{
		return UndefinedStateError(stateIdents, context);
	}

	llvm::Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	llvm::APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();

	// Load value from state variable, test against range of values
	llvm::Value* load = new llvm::LoadInst(topState.currentState, "", false, context.currentBlock());

	if (totalInBlock>1)
	{
		llvm::CmpInst* cmp = llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_UGE,load, context.getConstantInt(llvm::APInt(bitsNeeded,jumpIndex)), "", context.currentBlock());
		llvm::CmpInst* cmp2 = llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_ULT,load, context.getConstantInt(llvm::APInt(bitsNeeded,jumpIndex+totalInBlock)), "", context.currentBlock());
		return llvm::BinaryOperator::Create(llvm::Instruction::And,cmp,cmp2,"Combining",context.currentBlock());
	}
		
	// Load value from state variable, test against being equal to found index, jump on result
	return llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_EQ,load, context.getConstantInt(llvm::APInt(bitsNeeded,jumpIndex)), "", context.currentBlock());
}
