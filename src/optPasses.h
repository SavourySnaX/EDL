#pragma once

#include <llvm/Pass.h>

class CodeGenContext;

struct StateReferenceSquasher : public llvm::FunctionPass
{
	CodeGenContext *ourContext;

	static char ID;
	StateReferenceSquasher() : ourContext(nullptr), llvm::FunctionPass(ID) {}
	StateReferenceSquasher(CodeGenContext* context) : ourContext(context), llvm::FunctionPass(ID) {}

	bool DoWork(llvm::Function &F, CodeGenContext* context);
	virtual bool runOnFunction(llvm::Function &F);
};

// Below is similar to above - Essentially pin variables that are in cannot be modified during execution - this hoists all the loads into the entry block
struct InOutReferenceSquasher : public llvm::FunctionPass
{
	CodeGenContext *ourContext;

	static char ID;
	InOutReferenceSquasher() : ourContext(nullptr), llvm::FunctionPass(ID) {}
	InOutReferenceSquasher(CodeGenContext* context) : ourContext(context), llvm::FunctionPass(ID) {}

	bool DoWork(llvm::Function &F, CodeGenContext* context);
	virtual bool runOnFunction(llvm::Function &F);
};
