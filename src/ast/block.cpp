#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Value.h>

void CBlock::prePass(CodeGenContext& context)
{
	for (const auto& statement : statements)
	{
		statement->prePass(context);
	}
}

llvm::Value* CBlock::codeGen(CodeGenContext& context)
{
	llvm::Value *last = nullptr;
	for (const auto& statement : statements)
	{
		last = statement->codeGen(context);
		if (context.errorFlagged)
		{
			return nullptr;
		}
	}
	return last;
}
