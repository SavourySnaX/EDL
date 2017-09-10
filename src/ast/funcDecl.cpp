#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

void CFunctionDecl::prePass(CodeGenContext& context)
{
	block.prePass(context);
}

llvm::Value* CFunctionDecl::codeGen(CodeGenContext& context)
{
	int a;
	BitVariable returnVal;
	std::vector<llvm::Type*> FuncTy_8_args;

	for (a = 0; a < params.size(); a++)
	{
		unsigned size = params[a]->size.integer.getLimitedValue();
		FuncTy_8_args.push_back(llvm::IntegerType::get(TheContext, size));
	}
	llvm::FunctionType* FuncTy_8;

	if (returns.empty())
	{
		FuncTy_8 = llvm::FunctionType::get(llvm::Type::getVoidTy(TheContext), FuncTy_8_args, false);
	}
	else
	{
		unsigned size = returns[0]->size.integer.getLimitedValue();
		FuncTy_8 = llvm::FunctionType::get(llvm::IntegerType::get(TheContext, size), FuncTy_8_args, false);
	}

	llvm::Function* func = nullptr;
	if (internal || !context.isRoot)
	{
		func = llvm::Function::Create(FuncTy_8, llvm::GlobalValue::PrivateLinkage, context.symbolPrepend + name.name, context.module);
	}
	else
	{
		func = llvm::Function::Create(FuncTy_8, llvm::GlobalValue::ExternalLinkage, context.symbolPrepend + name.name, context.module);
		func->setCallingConv(llvm::CallingConv::C);
	}
	func->setDoesNotThrow();

	context.StartFunctionDebugInfo(func, functionLoc);

	context.m_externFunctions[name.name] = func;

	llvm::BasicBlock *bblock = llvm::BasicBlock::Create(TheContext, "entry", func, 0);

	context.pushBlock(bblock, block.blockStartLoc);

	if (!returns.empty())
	{
		returnVal = BitVariable(returns[0]->size.integer, 0);
		llvm::AllocaInst *alloc = new llvm::AllocaInst(llvm::Type::getIntNTy(TheContext, returns[0]->size.integer.getLimitedValue()), 0, returns[0]->id.name.c_str(), context.currentBlock());
		returnVal.value = alloc;
		context.locals()[returns[0]->id.name] = returnVal;
	}

	llvm::Function::arg_iterator args = func->arg_begin();
	a = 0;
	while (args != func->arg_end())
	{
		BitVariable temp(params[a]->size.integer,0);

		temp.value = &*args;
		temp.value->setName(params[a]->id.name);
		context.locals()[params[a]->id.name] = temp;
		a++;
		args++;
	}

	block.codeGen(context);

	if (returns.empty())
	{
		llvm::ReturnInst::Create(TheContext, context.currentBlock());			/* block may well have changed by time we reach here */
	}
	else
	{
		llvm::ReturnInst::Create(TheContext, new llvm::LoadInst(returnVal.value, "", context.currentBlock()), context.currentBlock());
	}

	context.popBlock(block.blockEndLoc);

	context.EndFunctionDebugInfo();

	return func;
}
