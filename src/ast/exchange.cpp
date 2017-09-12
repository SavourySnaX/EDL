#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

llvm::Value* CExchange::codeGen(CodeGenContext& context)
{
	BitVariable lhsVar, rhsVar;

	if (!context.LookupBitVariable(lhsVar, lhs.module, lhs.name, lhs.modLoc, lhs.nameLoc))
	{
		return nullptr;
	}
	if (!context.LookupBitVariable(rhsVar, rhs.module, rhs.name, rhs.modLoc, rhs.nameLoc))
	{
		return nullptr;
	}

	if (lhsVar.value->getType()->isPointerTy() && rhsVar.value->getType()->isPointerTy())
	{
		// Both sides must be identifiers

		llvm::Value* left = lhs.codeGen(context);
		llvm::Value* right = rhs.codeGen(context);

		if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
		{
			const llvm::IntegerType* leftType = llvm::cast<llvm::IntegerType>(left->getType());
			const llvm::IntegerType* rightType = llvm::cast<llvm::IntegerType>(right->getType());

			if (leftType->getBitWidth() != rightType->getBitWidth())
			{
				PrintErrorFromLocation(operatorLoc, "Both operands to exchange must be same size");
				context.FlagError();
				return nullptr;
			}

			CAssignment::generateAssignment(lhsVar, lhs, right, context);
			CAssignment::generateAssignment(rhsVar, rhs, left, context);
		}

		return nullptr;
	}

	PrintErrorFromLocation(operatorLoc, "Illegal operands to exchange");
	context.FlagError();
	return nullptr;
}
