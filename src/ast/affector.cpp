#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

void CAffector::prePass(CodeGenContext& context)
{
	expr.prePass(context);
}

llvm::Value* CAffector::codeGen(CodeGenContext& context)
{
	llvm::Value* left = nullptr;
	llvm::Value* right = nullptr;
	llvm::Value* exprResult = nullptr;
	bool containsCarryOverflow = false;
	int type = 0;

	if (context.curAffectors.size())
	{
		PrintErrorFromLocation(exprLoc, "(TODO) Cannot currently supply nested affectors");
		context.FlagError();
		return nullptr;
	}

	// If there is more than 1 expr that is used to compute CARRY/OVERFLOW, we need to test at each stage.

	for (int a = 0; a < affectors.size(); a++)
	{
		switch (affectors[a]->type)
		{
		case TOK_CARRY:
		case TOK_NOCARRY:
			containsCarryOverflow = true;
			affectors[a]->tmpResult = nullptr;
			context.curAffectors.push_back(affectors[a]);
			break;
		case TOK_OVERFLOW:
		case TOK_NOOVERFLOW:
			containsCarryOverflow = true;
			affectors[a]->tmpResult = nullptr;
			affectors[a]->ov1Val = affectors[a]->ov1.codeGen(context);
			affectors[a]->ov2Val = affectors[a]->ov2.codeGen(context);
			context.curAffectors.push_back(affectors[a]);
			break;
		default:
			break;
		}
	}

	if (containsCarryOverflow)
	{
		if (expr.IsLeaf())
		{
			PrintErrorFromLocation(exprLoc, "OVERFLOW/CARRY is not supported for non carry/overflow expressions");
			context.FlagError();
			return nullptr;
		}
	}

	exprResult = expr.codeGen(context);

	if (exprResult)
	{
		// Final result is in exprResult (+affectors for carry/overflow).. do final operations
		for (const auto& affector : affectors)
		{
			affector->codeGenFinal(context, exprResult);
		}
	}
	context.curAffectors.clear();

	return exprResult;
}
