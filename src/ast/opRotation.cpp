#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...);			// Todo refactor away

void CRotationOperator::prePass(CodeGenContext& context)
{
	value.prePass(context);
	bitsIn.prePass(context);
	rotAmount.prePass(context);
}

llvm::Value* CRotationOperator::codeGen(CodeGenContext& context)
{
	llvm::Value* toShift = value.codeGen(context);
	llvm::Value* shiftIn = bitsIn.codeGen(context);
	llvm::Value* rotBy = rotAmount.codeGen(context);
	BitVariable var;
	if (!context.LookupBitVariable(var, bitsOut.module, bitsOut.name, bitsOut.modLoc, bitsOut.nameLoc))
	{
		return nullptr;
	}

	if ((!toShift->getType()->isIntegerTy()) || (!rotBy->getType()->isIntegerTy()))
	{
		PrintErrorWholeLine(var.refLoc, "(TODO)Unsupported operation");
		context.FlagError();
		return nullptr;
	}

	if (var.mappingRef)
	{
		PrintErrorWholeLine(var.refLoc, "(TODO)Cannot perform operation on a mappingRef");
		context.FlagError();
		return nullptr;
	}

	llvm::IntegerType* toShiftType = llvm::cast<llvm::IntegerType>(toShift->getType());

	if (direction == TOK_ROL)
	{
		llvm::Instruction::CastOps oprotby = llvm::CastInst::getCastOpcode(rotBy, false, toShiftType, false);
		llvm::Value *rotByCast = llvm::CastInst::Create(oprotby, rotBy, toShiftType, "cast", context.currentBlock());

		llvm::Value *amountToShift = llvm::BinaryOperator::Create(llvm::Instruction::Sub, context.getConstantInt(llvm::APInt(toShiftType->getBitWidth(), toShiftType->getBitWidth())), rotByCast, "shiftAmount", context.currentBlock());

		llvm::Value *shiftedDown = llvm::BinaryOperator::Create(llvm::Instruction::LShr, toShift, amountToShift, "carryOutShift", context.currentBlock());

		CAssignment::generateAssignment(var, bitsOut, shiftedDown, context);

		llvm::Value *shifted = llvm::BinaryOperator::Create(llvm::Instruction::Shl, toShift, rotByCast, "rolShift", context.currentBlock());

		llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(shiftIn, false, toShiftType, false);

		llvm::Value *upCast = llvm::CastInst::Create(op, shiftIn, toShiftType, "cast", context.currentBlock());

		llvm::Value *rotResult = llvm::BinaryOperator::Create(llvm::Instruction::Or, upCast, shifted, "rolOr", context.currentBlock());

		return rotResult;
	}
	else
	{
		llvm::Instruction::CastOps oprotby = llvm::CastInst::getCastOpcode(rotBy, false, toShiftType, false);
		llvm::Value *rotByCast = llvm::CastInst::Create(oprotby, rotBy, toShiftType, "cast", context.currentBlock());

		llvm::Value *amountToShift = llvm::BinaryOperator::Create(llvm::Instruction::Sub, context.getConstantInt(llvm::APInt(toShiftType->getBitWidth(), toShiftType->getBitWidth())), rotByCast, "shiftAmount", context.currentBlock());

		llvm::APInt downMaskc(toShiftType->getBitWidth(), 0);
		downMaskc = ~downMaskc;
		llvm::Value *downMask = llvm::BinaryOperator::Create(llvm::Instruction::LShr, context.getConstantInt(downMaskc), amountToShift, "downmask", context.currentBlock());

		llvm::Value *maskedDown = llvm::BinaryOperator::Create(llvm::Instruction::And, toShift, downMask, "carryOutMask", context.currentBlock());

		CAssignment::generateAssignment(var, bitsOut, maskedDown, context);

		llvm::Value *shifted = llvm::BinaryOperator::Create(llvm::Instruction::LShr, toShift, rotByCast, "rorShift", context.currentBlock());

		llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(shiftIn, false, toShiftType, false);

		llvm::Value *upCast = llvm::CastInst::Create(op, shiftIn, toShiftType, "cast", context.currentBlock());

		llvm::Value *shiftedUp = llvm::BinaryOperator::Create(llvm::Instruction::Shl, upCast, amountToShift, "rorOrShift", context.currentBlock());

		llvm::Value *rotResult = llvm::BinaryOperator::Create(llvm::Instruction::Or, shiftedUp, shifted, "rorOr", context.currentBlock());

		return rotResult;
	}

	return value.codeGen(context);
}
