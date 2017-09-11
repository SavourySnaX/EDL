#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

CInteger CAffect::emptyParam("0");

llvm::Value* CAffect::codeGenCarry(CodeGenContext& context, llvm::Value* exprResult, llvm::Value* lhs, llvm::Value* rhs, int optype)
{
	llvm::IntegerType* resultType = llvm::cast<llvm::IntegerType>(exprResult->getType());
	llvm::Value *answer;
	switch (type)
	{
	case TOK_CARRY:
	case TOK_NOCARRY:
	{
		if (param.integer.getLimitedValue() >= resultType->getBitWidth())
		{
			PrintErrorFromLocation(param.integerLoc, "Bit to carry is outside of range for result");
			context.errorFlagged = true;
			return nullptr;
		}

		llvm::IntegerType* lhsType = llvm::cast<llvm::IntegerType>(lhs->getType());
		llvm::IntegerType* rhsType = llvm::cast<llvm::IntegerType>(rhs->getType());

		llvm::Instruction::CastOps lhsOp = llvm::CastInst::getCastOpcode(lhs, false, resultType, false);
		lhs = llvm::CastInst::Create(lhsOp, lhs, resultType, "cast", context.currentBlock());
		llvm::Instruction::CastOps rhsOp = llvm::CastInst::getCastOpcode(rhs, false, resultType, false);
		rhs = llvm::CastInst::Create(rhsOp, rhs, resultType, "cast", context.currentBlock());

		if (optype == TOK_ADD || optype == TOK_DADD)
		{
			//((lh & rh) | ((~Result) & rh) | (lh & (~Result)))

			// ~Result
			llvm::Value* cmpResult = llvm::BinaryOperator::Create(llvm::Instruction::Xor, exprResult, llvm::ConstantInt::get(TheContext, ~llvm::APInt(resultType->getBitWidth(), 0, false)), "", context.currentBlock());

			// lh&rh
			llvm::Value* expr1 = llvm::BinaryOperator::Create(llvm::Instruction::And, rhs, lhs, "", context.currentBlock());
			// ~Result & rh
			llvm::Value* expr2 = llvm::BinaryOperator::Create(llvm::Instruction::And, cmpResult, lhs, "", context.currentBlock());
			// lh & ~Result
			llvm::Value* expr3 = llvm::BinaryOperator::Create(llvm::Instruction::And, rhs, cmpResult, "", context.currentBlock());

			// lh&rh | ~Result&rh
			llvm::Value* combine = llvm::BinaryOperator::Create(llvm::Instruction::Or, expr1, expr2, "", context.currentBlock());
			// (lh&rh | ~Result&rh) | lh&~Result
			answer = llvm::BinaryOperator::Create(llvm::Instruction::Or, combine, expr3, "", context.currentBlock());
		}
		else
		{
			//((~lh&Result) | (~lh&rh) | (rh&Result)

			// ~lh
			llvm::Value* cmpLhs = llvm::BinaryOperator::Create(llvm::Instruction::Xor, lhs, llvm::ConstantInt::get(TheContext, ~llvm::APInt(resultType->getBitWidth(), 0, false)), "", context.currentBlock());

			// ~lh&Result
			llvm::Value* expr1 = llvm::BinaryOperator::Create(llvm::Instruction::And, cmpLhs, exprResult, "", context.currentBlock());
			// ~lh & rh
			llvm::Value* expr2 = llvm::BinaryOperator::Create(llvm::Instruction::And, cmpLhs, rhs, "", context.currentBlock());
			// rh & Result
			llvm::Value* expr3 = llvm::BinaryOperator::Create(llvm::Instruction::And, rhs, exprResult, "", context.currentBlock());

			// ~lh&Result | ~lh&rh
			llvm::Value* combine = llvm::BinaryOperator::Create(llvm::Instruction::Or, expr1, expr2, "", context.currentBlock());
			// (~lh&Result | ~lh&rh) | rh&Result
			answer = llvm::BinaryOperator::Create(llvm::Instruction::Or, combine, expr3, "", context.currentBlock());
		}
	}
	break;

	default:
		assert(0 && "Unknown affector");
		break;
	}

	if (tmpResult)
	{
		tmpResult = llvm::BinaryOperator::Create(llvm::Instruction::Or, answer, tmpResult, "", context.currentBlock());
	}
	else
	{
		tmpResult = answer;
	}

	return tmpResult;
}

llvm::Value* CAffect::codeGenFinal(CodeGenContext& context, llvm::Value* exprResult)
{
	BitVariable var;
	if (!context.LookupBitVariable(var, ident.module, ident.name, ident.modLoc, ident.nameLoc))
	{
		return nullptr;
	}

	if (var.mappingRef)
	{
		PrintErrorFromLocation(var.refLoc, "Cannot perform operation on a mapping reference");
		context.errorFlagged = true;
		return nullptr;
	}

	llvm::IntegerType* resultType = llvm::cast<llvm::IntegerType>(exprResult->getType());
	llvm::Value *answer;
	switch (type)
	{
	case TOK_FORCESET:
		answer = llvm::ConstantInt::get(TheContext, ~llvm::APInt(resultType->getBitWidth(), 0, false));
		break;
	case TOK_FORCERESET:
		answer = llvm::ConstantInt::get(TheContext, llvm::APInt(resultType->getBitWidth(), 0, false));
		break;
	case TOK_ZERO:
		answer = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_EQ, exprResult, llvm::ConstantInt::get(TheContext, llvm::APInt(resultType->getBitWidth(), 0, false)), "", context.currentBlock());
		break;
	case TOK_NONZERO:
		answer = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_NE, exprResult, llvm::ConstantInt::get(TheContext, llvm::APInt(resultType->getBitWidth(), 0, false)), "", context.currentBlock());
		break;
	case TOK_SIGN:
	case TOK_NOSIGN:
	{
		llvm::APInt signBit(resultType->getBitWidth(), 0, false);
		signBit.setBit(resultType->getBitWidth() - 1);
		if (type == TOK_SIGN)
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::And, exprResult, llvm::ConstantInt::get(TheContext, signBit), "", context.currentBlock());
		}
		else
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::Xor, exprResult, llvm::ConstantInt::get(TheContext, signBit), "", context.currentBlock());
		}
		answer = llvm::BinaryOperator::Create(llvm::Instruction::LShr, answer, llvm::ConstantInt::get(TheContext, llvm::APInt(resultType->getBitWidth(), (uint64_t)(resultType->getBitWidth() - 1), false)), "", context.currentBlock());
	}
	break;
	case TOK_PARITYODD:
	case TOK_PARITYEVEN:

		// To generate the parity of a number, we use the parallel method. minimum width is 4 bits then for each ^2 bit size we add an extra shift operation
	{
		llvm::APInt computeClosestPower2Size(3, "100", 2);
		unsigned count = 0;
		while (true)
		{
			if (resultType->getBitWidth() <= computeClosestPower2Size.getLimitedValue())
			{
				// Output a casting operator to up the size of type
				llvm::Type* ty = llvm::Type::getIntNTy(TheContext, computeClosestPower2Size.getLimitedValue());
				llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(exprResult, false, ty, false);
				answer = llvm::CastInst::Create(op, exprResult, ty, "", context.currentBlock());
				break;
			}

			count++;
			computeClosestPower2Size = computeClosestPower2Size.zext(3 + count);
			computeClosestPower2Size = computeClosestPower2Size.shl(1);			//TODO fix infinite loop if we ever create more bits than APInt can handle! 
		}

		// answer is now appropriate size, next step for each count shrink the bits so we can test a nibble

		computeClosestPower2Size = computeClosestPower2Size.zext(computeClosestPower2Size.getLimitedValue());
		computeClosestPower2Size = computeClosestPower2Size.lshr(1);
		for (int a = 0; a < count; a++)
		{
			llvm::Value *shifted = llvm::BinaryOperator::Create(llvm::Instruction::LShr, answer, llvm::ConstantInt::get(TheContext, computeClosestPower2Size), "", context.currentBlock());
			answer = llvm::BinaryOperator::Create(llvm::Instruction::Xor, shifted, answer, "", context.currentBlock());
			computeClosestPower2Size = computeClosestPower2Size.lshr(1);
		}

		// final part, mask to nibble size and use this to lookup into magic constant 0x6996 (which is simply a table look up for the parities of a nibble)
		answer = llvm::BinaryOperator::Create(llvm::Instruction::And, answer, llvm::ConstantInt::get(TheContext, llvm::APInt(computeClosestPower2Size.getBitWidth(), 0xF, false)), "", context.currentBlock());
		llvm::Type* ty = llvm::Type::getIntNTy(TheContext, 16);
		llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(answer, false, ty, false);
		answer = llvm::CastInst::Create(op, answer, ty, "", context.currentBlock());
		if (type == TOK_PARITYEVEN)
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::LShr, llvm::ConstantInt::get(TheContext, ~llvm::APInt(16, 0x6996, false)), answer, "", context.currentBlock());
		}
		else
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::LShr, llvm::ConstantInt::get(TheContext, llvm::APInt(16, 0x6996, false)), answer, "", context.currentBlock());
		}
	}
	break;
	case TOK_BIT:
	case TOK_INVBIT:
	{
		llvm::APInt bit(resultType->getBitWidth(), 0, false);
		llvm::APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
		bit.setBit(param.integer.getLimitedValue());
		if (type == TOK_BIT)
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::And, exprResult, llvm::ConstantInt::get(TheContext, bit), "", context.currentBlock());
		}
		else
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::Xor, exprResult, llvm::ConstantInt::get(TheContext, bit), "", context.currentBlock());
		}
		answer = llvm::BinaryOperator::Create(llvm::Instruction::LShr, answer, llvm::ConstantInt::get(TheContext, shift), "", context.currentBlock());
	}
	break;
	case TOK_OVERFLOW:
	case TOK_NOOVERFLOW:
	{
		llvm::APInt bit(resultType->getBitWidth(), 0, false);
		llvm::APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
		bit.setBit(param.integer.getLimitedValue());
		llvm::ConstantInt* bitC = llvm::ConstantInt::get(TheContext, bit);
		llvm::ConstantInt* shiftC = llvm::ConstantInt::get(TheContext, shift);

		if (param.integer.getLimitedValue() >= resultType->getBitWidth())
		{
			PrintErrorFromLocation(param.integerLoc, "Bit for overflow detection is outside of range for result");
			context.errorFlagged = true;
			return nullptr;
		}
		llvm::Value* lhs = ov1Val;		// Lhs and Rhs are provided in the overflow affector
		if (lhs == nullptr)
		{
			context.errorFlagged = true;
			return nullptr;
		}
		llvm::Value* rhs = ov2Val;
		if (rhs == nullptr)
		{
			context.errorFlagged = true;
			return nullptr;
		}

		llvm::IntegerType* lhsType = llvm::cast<llvm::IntegerType>(lhs->getType());
		llvm::IntegerType* rhsType = llvm::cast<llvm::IntegerType>(rhs->getType());

		if (param.integer.getLimitedValue() >= lhsType->getBitWidth())
		{
			PrintErrorFromLocation(param.integerLoc, "Bit for overflow detection is outside of range of lhs");
			std::cerr << "Bit for overflow detection is outside of range for source1" << std::endl;
			context.errorFlagged = true;
			return nullptr;
		}
		if (param.integer.getLimitedValue() >= rhsType->getBitWidth())
		{
			PrintErrorFromLocation(param.integerLoc, "Bit for overflow detection is outside of range of rhs");
			context.errorFlagged = true;
			return nullptr;
		}

		llvm::Instruction::CastOps lhsOp = llvm::CastInst::getCastOpcode(lhs, false, resultType, false);
		lhs = llvm::CastInst::Create(lhsOp, lhs, resultType, "cast", context.currentBlock());
		llvm::Instruction::CastOps rhsOp = llvm::CastInst::getCastOpcode(rhs, false, resultType, false);
		rhs = llvm::CastInst::Create(rhsOp, rhs, resultType, "cast", context.currentBlock());

		// ~Result
		llvm::Value* invResult = llvm::BinaryOperator::Create(llvm::Instruction::Xor, exprResult, llvm::ConstantInt::get(TheContext, ~llvm::APInt(resultType->getBitWidth(), 0, false)), "", context.currentBlock());
		// ~lh
		llvm::Value* invlh = llvm::BinaryOperator::Create(llvm::Instruction::Xor, lhs, llvm::ConstantInt::get(TheContext, ~llvm::APInt(resultType->getBitWidth(), 0, false)), "", context.currentBlock());
		// ~rh
		llvm::Value* invrh = llvm::BinaryOperator::Create(llvm::Instruction::Xor, rhs, llvm::ConstantInt::get(TheContext, ~llvm::APInt(resultType->getBitWidth(), 0, false)), "", context.currentBlock());

		llvm::Value* expr1;
		llvm::Value* expr2;

		if (opType == TOK_ADD || opType == TOK_DADD)
		{
			//((rh & lh & (~Result)) | ((~rh) & (~lh) & Result))
			expr1 = llvm::BinaryOperator::Create(llvm::Instruction::And, rhs, lhs, "", context.currentBlock());
			expr2 = llvm::BinaryOperator::Create(llvm::Instruction::And, invrh, invlh, "", context.currentBlock());
		}
		else
		{
			//(((~rh) & lh & (~Result)) | (rh & (~lh) & Result))
			expr1 = llvm::BinaryOperator::Create(llvm::Instruction::And, invrh, lhs, "", context.currentBlock());
			expr2 = llvm::BinaryOperator::Create(llvm::Instruction::And, rhs, invlh, "", context.currentBlock());

		}
		llvm::Value* expr3 = llvm::BinaryOperator::Create(llvm::Instruction::And, expr1, invResult, "", context.currentBlock());
		llvm::Value* expr4 = llvm::BinaryOperator::Create(llvm::Instruction::And, expr2, exprResult, "", context.currentBlock());

		answer = llvm::BinaryOperator::Create(llvm::Instruction::Or, expr3, expr4, "", context.currentBlock());
		if (type == TOK_OVERFLOW)
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::And, answer, bitC, "", context.currentBlock());
		}
		else
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::Xor, answer, bitC, "", context.currentBlock());
		}
		answer = llvm::BinaryOperator::Create(llvm::Instruction::LShr, answer, shiftC, "", context.currentBlock());
	}
	break;
	case TOK_CARRY:
	case TOK_NOCARRY:
	{
		llvm::APInt bit(resultType->getBitWidth(), 0, false);
		llvm::APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
		bit.setBit(param.integer.getLimitedValue());
		llvm::ConstantInt* bitC = llvm::ConstantInt::get(TheContext, bit);
		llvm::ConstantInt* shiftC = llvm::ConstantInt::get(TheContext, shift);

		if (tmpResult == nullptr)
		{
			assert(0 && "Failed to compute CARRY/OVERFLOW for expression (possible bug in compiler)");
		}

		if (type == TOK_CARRY)
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::And, tmpResult, bitC, "", context.currentBlock());
		}
		else
		{
			answer = llvm::BinaryOperator::Create(llvm::Instruction::Xor, tmpResult, bitC, "", context.currentBlock());
		}
		answer = llvm::BinaryOperator::Create(llvm::Instruction::LShr, answer, shiftC, "", context.currentBlock());
	}
	break;

	default:
		assert(0 && "Unknown affector");
		break;
	}

	return CAssignment::generateAssignment(var, ident, answer, context);
}
