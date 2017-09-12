#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away
extern void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...);			// Todo refactor away

void CFuncCall::prePass(CodeGenContext& context)
{
	for (int a = 0; a < params.size(); a++)
	{
		params[a]->prePass(context);
	}
}

llvm::Value* CFuncCall::codeGen(CodeGenContext& context)
{
	if (context.m_externFunctions.find(name.name) == context.m_externFunctions.end())
	{
		PrintErrorFromLocation(name.nameLoc, "Function Declaration Required to use a Function");
		context.FlagError();
		return nullptr;
	}
	llvm::Function* func = context.m_externFunctions[name.name];

	std::vector<llvm::Value*> args;
	const llvm::FunctionType* funcType = func->getFunctionType();

	for (int a = 0, b = 0; a < params.size(); a++, b++)
	{
		if (b == funcType->getNumParams())
		{
			PrintErrorWholeLine(name.nameLoc, "Wrong Number Of Arguments To Function");
			context.FlagError();
			return nullptr;
		}

		llvm::Value* exprResult = params[a]->codeGen(context);

		// We need to promote the return type to the same as the function parameters (Auto promote rules)
		llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(exprResult, false, funcType->getParamType(b), false);
		exprResult = llvm::CastInst::Create(op, exprResult, funcType->getParamType(b), "", context.currentBlock());

		args.push_back(exprResult);
	}
	llvm::Value* callReturn;
	if (funcType->getReturnType()->isVoidTy())
	{
		llvm::Instruction* call = llvm::CallInst::Create(func, args, "", context.currentBlock());
		if (context.gContext.opts.generateDebug)
		{
			call->setDebugLoc(llvm::DebugLoc::get(callLoc.first_line, callLoc.first_column, context.gContext.scopingStack.top()));
		}

		callReturn = context.getConstantZero(1);		// Forces void returns to actually return 0
	}
	else
	{
		llvm::Instruction* call = llvm::CallInst::Create(func, args, "CallingCFunc" + name.name, context.currentBlock());
		if (context.gContext.opts.generateDebug)
		{
			call->setDebugLoc(llvm::DebugLoc::get(callLoc.first_line, callLoc.first_column, context.gContext.scopingStack.top()));
		}
		callReturn = call;
	}

	return callReturn;
}
