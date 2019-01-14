#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

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
            llvm::APInt width = end.getAPInt().zextOrTrunc(leftType->getBitWidth());
            width++;
            width -= start;
			while (1 == 1)
			{
				mask.setBit(loop.getLimitedValue());
				if (loop.uge(endloop))
					break;
				loop++;
			}

			llvm::Value *masked = llvm::BinaryOperator::Create(llvm::Instruction::And, left, context.getConstantInt(mask), "castMask", context.currentBlock());
			llvm::Value *shifted = llvm::BinaryOperator::Create(llvm::Instruction::LShr, masked, context.getConstantInt(start), "castShift", context.currentBlock());

			// Final step cast it to correct size (bug fix, we didn't use to do this, but DECLARE VAR32; $0000++VAR[0..7]->VAR32;  would result internally as $0000++(VAR & 0xFF)  since VAR is greater than $0000 we sign extend the wrong side
            llvm::Type *limited = context.getIntType(width);
            llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(shifted, false, limited, false);
            shifted = llvm::CastInst::Create(op, shifted, limited, "castLimitFix", context.currentBlock());

			return shifted;
		}
	}

	return context.gContext.ReportError(nullptr, EC_InternalError, operatorLoc, "(TODO)Illegal type in cast");
}
