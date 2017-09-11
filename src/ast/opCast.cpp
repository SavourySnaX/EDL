#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

CInteger CCastOperator::begZero("0");

void CCastOperator::prePass(CodeGenContext& context)
{
	lhs.prePass(context);
}

llvm::Value* CCastOperator::codeGen(CodeGenContext& context)
{
	// Compute a mask between specified bits, shift result down by start bits
	if (lhs.IsAssignmentExpression())
	{
		CAssignment* assign = (CAssignment*)&lhs;
		return assign->codeGen(context, this);
	}
	else
	{
		llvm::Value* left = lhs.codeGen(context);
		if (left && left->getType()->isIntegerTy())
		{
			const llvm::IntegerType* leftType = llvm::cast<llvm::IntegerType>(left->getType());

			llvm::APInt mask(leftType->getBitWidth(), "0", 10);
			llvm::APInt start = beg.getAPInt().zextOrTrunc(leftType->getBitWidth());
			llvm::APInt loop = start;
			llvm::APInt endloop = end.getAPInt().zextOrTrunc(leftType->getBitWidth());

			while (1 == 1)
			{
				mask.setBit(loop.getLimitedValue());
				if (loop.uge(endloop))
					break;
				loop++;
			}

			llvm::Value *masked = llvm::BinaryOperator::Create(llvm::Instruction::And, left, context.getConstantInt(mask), "castMask", context.currentBlock());
			llvm::Value *shifted = llvm::BinaryOperator::Create(llvm::Instruction::LShr, masked, context.getConstantInt(start), "castShift", context.currentBlock());

			// Final step cast it to correct size - not actually required, will be handled by expr lowering/raising anyway
			return shifted;
		}
	}

	PrintErrorFromLocation(operatorLoc, "(TODO)Illegal type in cast");
	context.errorFlagged = true;
	return nullptr;
}
