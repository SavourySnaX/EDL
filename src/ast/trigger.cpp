#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

CInteger CTrigger::zero("0");

llvm::Value* CTrigger::GenerateAlways(CodeGenContext& context, BitVariable& pin, llvm::Value* function)
{
	llvm::BasicBlock *oldBlock = (*pin.writeAccessor)->getParent();
	llvm::Function* parentFunction = oldBlock->getParent();
	context.gContext.scopingStack.push(parentFunction->getSubprogram());

	llvm::CallInst* fcall = llvm::CallInst::Create(function, "", *pin.writeAccessor);
	if (context.gContext.opts.generateDebug)
	{
		fcall->setDebugLoc(llvm::DebugLoc::get(debugLocation.first_line, debugLocation.first_column, context.gContext.scopingStack.top()));
	}

	context.gContext.scopingStack.pop();

	return nullptr;
}

llvm::Value* CTrigger::GenerateChanged(CodeGenContext& context, BitVariable& pin, llvm::Value* function)
{
	// Compare input value to old value, if different then call function 
	llvm::BasicBlock *oldBlock = (*pin.writeAccessor)->getParent();
	llvm::Function* parentFunction = oldBlock->getParent();
	context.gContext.scopingStack.push(parentFunction->getSubprogram());

	context.pushBlock(oldBlock, debugLocation);
	llvm::Value* prior = CIdentifier::GetAliasedData(context, pin.priorValue, pin);
	llvm::Value* writeInput = CIdentifier::GetAliasedData(context, pin.writeInput, pin);
	llvm::Value* answer = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_EQ, prior, writeInput, "", context.currentBlock());

	llvm::BasicBlock *ifcall = context.makeBasicBlock("ifcall", parentFunction);
	llvm::BasicBlock *nocall = context.makeBasicBlock("nocall", parentFunction);

	llvm::BranchInst::Create(nocall, ifcall, answer, context.currentBlock());
	context.popBlock(debugLocation);

	llvm::CallInst* fcall = llvm::CallInst::Create(function, "", ifcall);
	if (context.gContext.opts.generateDebug)
	{
		fcall->setDebugLoc(llvm::DebugLoc::get(debugLocation.first_line, debugLocation.first_column, context.gContext.scopingStack.top()));
	}
	llvm::BranchInst::Create(nocall, ifcall);

	// Remove return instruction (since we need to create a new basic block set
	(*pin.writeAccessor)->removeFromParent();

	*pin.writeAccessor = context.makeReturn(nocall);

	context.gContext.scopingStack.pop();

	return nullptr;
}

llvm::Value* CTrigger::GenerateTransition(CodeGenContext& context, BitVariable& pin, llvm::Value* function)
{
	// Compare input value to param2 and old value to param1 , if same call function 
	llvm::BasicBlock *oldBlock = (*pin.writeAccessor)->getParent();
	llvm::Function* parentFunction = oldBlock->getParent();
	context.gContext.scopingStack.push(parentFunction->getSubprogram());

	context.pushBlock(oldBlock, debugLocation);
	llvm::Value* prior = CIdentifier::GetAliasedData(context, pin.priorValue, pin);
	llvm::Value* writeInput = CIdentifier::GetAliasedData(context, pin.writeInput, pin);

	llvm::Value* checkParam2 = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_EQ, context.getConstantInt(param2.getAPInt().zextOrTrunc(pin.trueSize.getLimitedValue())), writeInput, "", context.currentBlock());
	llvm::Value* checkParam1 = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_EQ, context.getConstantInt(param1.getAPInt().zextOrTrunc(pin.trueSize.getLimitedValue())), prior, "", context.currentBlock());

	llvm::Value *answer = llvm::BinaryOperator::Create(llvm::Instruction::And, checkParam1, checkParam2, "", context.currentBlock());

	llvm::BasicBlock *ifcall = context.makeBasicBlock("ifcall", parentFunction);
	llvm::BasicBlock *nocall = context.makeBasicBlock("nocall", parentFunction);

	llvm::BranchInst::Create(ifcall, nocall, answer, context.currentBlock());
	context.popBlock(debugLocation);

	llvm::CallInst* fcall = llvm::CallInst::Create(function, "", ifcall);
	if (context.gContext.opts.generateDebug)
	{
		fcall->setDebugLoc(llvm::DebugLoc::get(debugLocation.first_line, debugLocation.first_column, context.gContext.scopingStack.top()));
	}
	llvm::BranchInst::Create(nocall, ifcall);

	// Remove return instruction (since we need to create a new basic block set
	(*pin.writeAccessor)->removeFromParent();

	*pin.writeAccessor = context.makeReturn(nocall);

	context.gContext.scopingStack.pop();

	return nullptr;
}

llvm::Value* CTrigger::codeGen(CodeGenContext& context, BitVariable& pin, llvm::Value* function)
{
	switch (type)
	{
	case TOK_ALWAYS:
		return GenerateAlways(context, pin, function);
	case TOK_CHANGED:
		return GenerateChanged(context, pin, function);
	case TOK_TRANSITION:
		return GenerateTransition(context, pin, function);
	default:
		assert(0 && "Unhandled trigger type");
		break;
	}

	return nullptr;
}
