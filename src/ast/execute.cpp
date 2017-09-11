#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Instructions.h>

CIdentifier CExecute::emptyTable("");

void CExecute::prePass(CodeGenContext& context)
{
	// this will allow us to have instructions before execute and vice versa
}

llvm::Value* CExecute::codeGen(CodeGenContext& context)
{
	llvm::Value* load = opcode.codeGen(context);

	if (load->getType()->isIntegerTy())
	{
		const llvm::IntegerType* typeForExecute = llvm::cast<llvm::IntegerType>(load->getType());

		ExecuteInformation temp;

		temp.executeLoc = executeLoc;
		temp.blockEndForExecute = context.makeBasicBlock("execReturn", context.currentBlock()->getParent());

		if (context.gContext.opts.traceUnimplemented)
		{
			llvm::BasicBlock* tempBlock = context.makeBasicBlock("default", context.currentBlock()->getParent());

			std::vector<llvm::Value*> args;

			// Handle variable promotion
			llvm::Type* ty = context.getIntType(32);
			llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(load, false, ty, false);

			llvm::Instruction* truncExt = llvm::CastInst::Create(op, load, ty, "cast", tempBlock);

			args.push_back(truncExt);
			llvm::CallInst *call = llvm::CallInst::Create(context.debugTraceMissing, args, "DEBUGTRACEMISSING", tempBlock);

			llvm::BranchInst::Create(temp.blockEndForExecute, tempBlock);

			temp.switchForExecute = llvm::SwitchInst::Create(load, tempBlock, 2 << typeForExecute->getBitWidth(), context.currentBlock());
		}
		else
		{
			temp.switchForExecute = llvm::SwitchInst::Create(load, temp.blockEndForExecute, 2 << typeForExecute->getBitWidth(), context.currentBlock());
		}

		context.setBlock(temp.blockEndForExecute);

		context.executeLocations[table.name].push_back(temp);
	}
	return nullptr;
}
