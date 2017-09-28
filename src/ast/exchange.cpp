#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

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

	if ((lhsVar.mappingRef || lhsVar.value->getType()->isPointerTy()) && (rhsVar.mappingRef || rhsVar.value->getType()->isPointerTy()))
	{
		// Both sides must be identifiers/valid mappingReferences
		llvm::Value* left = nullptr;
		llvm::Value* right = nullptr;

		if (lhsVar.mappingRef)
		{
			left = lhsVar.mapping->generateCallGetByMapping(context, lhs.modLoc, lhs.nameLoc);
		}
		else
		{
			left = lhs.codeGen(context);
		}

		if (rhsVar.mappingRef)
		{
			right = rhsVar.mapping->generateCallGetByMapping(context, rhs.modLoc, rhs.nameLoc);
		}
		else
		{
			right = rhs.codeGen(context);
		}

		if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
		{
			const llvm::IntegerType* leftType = llvm::cast<llvm::IntegerType>(left->getType());
			const llvm::IntegerType* rightType = llvm::cast<llvm::IntegerType>(right->getType());

			if (leftType->getBitWidth() != rightType->getBitWidth())
			{
				return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, operatorLoc, "Both operands to exchange must be same size");
			}

			if (lhsVar.mappingRef)
			{
				lhsVar.mapping->generateCallSetByMapping(context, lhs.modLoc, lhs.nameLoc, right);
			}
			else
			{
				CAssignment::generateAssignment(lhsVar, lhs, right, context);
			}
			if (rhsVar.mappingRef)
			{
				rhsVar.mapping->generateCallSetByMapping(context, rhs.modLoc, rhs.nameLoc, left);
			}
			else
			{
				CAssignment::generateAssignment(rhsVar, rhs, left, context);
			}
			return nullptr;
		}
	}

	return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, operatorLoc, "Illegal operands to exchange (must both be assignable)");
}
