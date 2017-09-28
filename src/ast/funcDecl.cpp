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
	// Declare function in pre-pass, allows use before definition
	int a;
	std::vector<llvm::Type*> FuncTy_8_args;

	for (const auto& param : params)
	{
		FuncTy_8_args.push_back(context.getIntType(param->size));
	}
	llvm::FunctionType* FuncTy_8;

	if (returns.empty())
	{
		FuncTy_8 = llvm::FunctionType::get(context.getVoidType(), FuncTy_8_args, false);
	}
	else
	{
		FuncTy_8 = llvm::FunctionType::get(context.getIntType(returns[0]->size), FuncTy_8_args, false);
	}

	llvm::Function* func = nullptr;
	if (internal || !context.isRoot)
	{
		func = context.makeFunction(FuncTy_8, llvm::GlobalValue::PrivateLinkage, context.getSymbolPrefix() + name.name);
	}
	else
	{
		func = context.makeFunction(FuncTy_8, llvm::GlobalValue::ExternalLinkage, context.getSymbolPrefix() + name.name);
	}

	context.m_externFunctions[name.name] = func;

	funcDecl = func;

	block.prePass(context);
}

llvm::Value* CFunctionDecl::codeGen(CodeGenContext& context)
{
	BitVariable returnVal;

	context.StartFunctionDebugInfo(funcDecl, functionLoc);

	llvm::BasicBlock *bblock = context.makeBasicBlock("entry", funcDecl);

	context.pushBlock(bblock, block.blockStartLoc);

	if (!returns.empty())
	{
		returnVal = BitVariable(returns[0]->size.getAPInt(), 0);
		llvm::AllocaInst *alloc = new llvm::AllocaInst(context.getIntType(returns[0]->size), 0, returns[0]->id.name.c_str(), context.currentBlock());
		returnVal.value = alloc;
		context.locals()[returns[0]->id.name] = returnVal;
	}

	llvm::Function::arg_iterator args = funcDecl->arg_begin();
	int a = 0;
	while (args != funcDecl->arg_end())
	{
		BitVariable temp(params[a]->size.getAPInt(),0);

		temp.value = &*args;
		temp.value->setName(params[a]->id.name);
		context.locals()[params[a]->id.name] = temp;
		a++;
		args++;
	}

	block.codeGen(context);

	if (returns.empty())
	{
		context.makeReturn(context.currentBlock());
	}
	else
	{
		context.makeReturnValue(new llvm::LoadInst(returnVal.value, "", context.currentBlock()), context.currentBlock());
	}

	context.popBlock(block.blockEndLoc);

	context.EndFunctionDebugInfo();

	return funcDecl;
}
