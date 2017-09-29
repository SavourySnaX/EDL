#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

void CIfStatement::prePass(CodeGenContext& context)
{
	block.prePass(context);
	elseBlock.prePass(context);
}

llvm::Value* CIfStatement::codeGen(CodeGenContext& context)
{
	llvm::BasicBlock *then = context.makeBasicBlock("then", context.currentBlock()->getParent());
	llvm::BasicBlock *endif = context.makeBasicBlock("endif", context.currentBlock()->getParent());
	llvm::BasicBlock *elsebb = nullptr;
	if (!elseBlock.statements.empty())
	{
		elsebb = context.makeBasicBlock("else", context.currentBlock()->getParent());
	}

	llvm::Value* result = expr.codeGen(context);
	if (result != nullptr)
	{
		if (elsebb == nullptr)
		{
			llvm::BranchInst::Create(then, endif, result, context.currentBlock());
		}
		else
		{
			llvm::BranchInst::Create(then, elsebb, result, context.currentBlock());
		}

		context.pushBlock(then, block.blockStartLoc);

		block.codeGen(context);
		llvm::BranchInst::Create(endif, context.currentBlock());

		context.popBlock(block.blockEndLoc);

		if (elsebb != nullptr)
		{
			context.pushBlock(elsebb, elseBlock.blockStartLoc);

			elseBlock.codeGen(context);
			llvm::BranchInst::Create(endif, context.currentBlock());

			context.popBlock(elseBlock.blockEndLoc);
		}

		context.setBlock(endif);
	}
	return nullptr;
}
