#include "yyltype.h"
#include "ast.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>

int CStateDeclaration::GetNumStates() const
{
	if (child == nullptr)
	{
		return 1;
	}

	return child->GetNumStates();
}

bool CStateDeclaration::FindStateIdx(CStatesDeclaration* find, int& idx) const
{
	if (child == find)
		return true;
	if (child == nullptr)
	{
		idx += 1;
		return false;
	}
	return child->FindStateIdx(find,idx);
}

llvm::Value* CStateDeclaration::codeGen(CodeGenContext& context, llvm::Function* parent)
{
	entry = llvm::BasicBlock::Create(TheContext, id.name + "entry", parent);
	exit = llvm::BasicBlock::Create(TheContext, id.name + "exit", parent);

	return nullptr;
}
