#include "yyltype.h"
#include "ast.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>

llvm::Value* CStateDeclaration::codeGen(CodeGenContext& context, llvm::Function* parent)
{
	entry = llvm::BasicBlock::Create(TheContext, id.name + "entry", parent);
	exit = llvm::BasicBlock::Create(TheContext, id.name + "exit", parent);

	return nullptr;
}
