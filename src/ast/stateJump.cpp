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

void CStateJump::prePass(CodeGenContext& context)
{

}

llvm::Value* CStateJump::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE." + stateIdents[0]->name;

	if (context.states().find(stateLabel) == context.states().end())
	{
		PrintErrorFromLocation(stateIdents[0]->nameLoc, "Unknown handler, can't look up state reference");
		context.FlagError();
		return nullptr;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates = topState.decl->GetNumStates();
	int jumpIndex;
	if (!topState.decl->FindStateIdx(stateIdents, jumpIndex))
	{
		return UndefinedStateError(stateIdents, context);
	}

	llvm::Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	llvm::APInt overSized(4 * numStates.length(), numStates, 10);
	unsigned bitsNeeded = overSized.getActiveBits();

	return new llvm::StoreInst(context.getConstantInt(llvm::APInt(bitsNeeded, jumpIndex)), topState.nextState, false, context.currentBlock());
}
