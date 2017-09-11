#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

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
	entry = context.makeBasicBlock(id.name + "entry", parent);
	exit = context.makeBasicBlock(id.name + "exit", parent);

	return nullptr;
}
