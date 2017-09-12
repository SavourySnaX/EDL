#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

void CHandlerDeclaration::prePass(CodeGenContext& context)
{
	block.prePass(context);
}

llvm::Value* CHandlerDeclaration::codeGen(CodeGenContext& context)
{
	std::vector<llvm::Type*> argTypes;
	BitVariable pinVariable;

	if (context.globals().find(id.name) == context.globals().end())
	{
		PrintErrorFromLocation(id.nameLoc, "undeclared pin %s - Handlers MUST be tied to pin definitions", id.name.c_str());
		context.FlagError();
		return nullptr;
	}

	pinVariable = context.globals()[id.name];

	if (pinVariable.writeAccessor == nullptr)
	{
		PrintErrorFromLocation(id.nameLoc, "Handlers must be tied to Input / Bidirectional pins ONLY");
		context.FlagError();
		return nullptr;
	}

	llvm::FunctionType *ftype = llvm::FunctionType::get(context.getVoidType(), argTypes, false);
	llvm::Function* function = context.makeFunction(ftype, llvm::GlobalValue::PrivateLinkage, context.moduleName + context.getSymbolPrefix() + "HANDLER." + id.name);

	context.StartFunctionDebugInfo(function, handlerLoc);

	llvm::BasicBlock *bblock = context.makeBasicBlock("entry", function);

	context.m_handlers[id.name] = this;

	context.pushBlock(bblock, block.blockStartLoc);

	std::string oldStateLabelStack = context.stateLabelStack;
	context.stateLabelStack += "." + id.name;

	context.parentHandler = this;

	trigger.codeGen(context, pinVariable, function);

	block.codeGen(context);

	context.parentHandler = nullptr;

	llvm::Instruction* I = context.makeReturn(context.currentBlock());
	if (context.gContext.opts.generateDebug)
	{
		I->setDebugLoc(llvm::DebugLoc::get(block.blockEndLoc.first_line, block.blockEndLoc.first_column, context.gContext.scopingStack.top()));
	}

	context.stateLabelStack = oldStateLabelStack;

	context.popBlock(block.blockEndLoc);

	context.EndFunctionDebugInfo();

	return function;
}
