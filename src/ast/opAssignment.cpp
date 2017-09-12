#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

llvm::Instruction* CAssignment::generateImpedanceAssignment(BitVariable& to, llvm::Value* assignTo, CodeGenContext& context)
{
	llvm::ConstantInt* impedance = context.getConstantOnes(to.size.getLimitedValue());

	return new llvm::StoreInst(impedance, assignTo, false, context.currentBlock());
}

llvm::Instruction* CAssignment::generateAssignment(BitVariable& to, const CIdentifier& toIdent, llvm::Value* from, CodeGenContext& context, bool isVolatile)
{
	return generateAssignmentActual(to, toIdent, from, context, true, isVolatile);
}


llvm::Instruction* CAssignment::generateAssignmentActual(BitVariable& to, const CIdentifier& toIdent, llvm::Value* from, CodeGenContext& context, bool clearImpedance, bool isVolatile)
{
	llvm::Value* assignTo;

	assignTo = to.value;

	if (toIdent.IsArray())
	{
		llvm::Value* exprResult = toIdent.GetExpression()->codeGen(context);

		llvm::Type* ty = context.getIntType(to.arraySize.getLimitedValue());
		llvm::Type* ty64 = context.getIntType(64);
		llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(exprResult, false, ty, false);
		llvm::Instruction* truncExt0 = llvm::CastInst::Create(op, exprResult, ty, "cast", context.currentBlock());		// Cast index to 64 bit type
		llvm::Instruction::CastOps op1 = llvm::CastInst::getCastOpcode(exprResult, false, ty64, false);
		llvm::Instruction* truncExt = llvm::CastInst::Create(op1, truncExt0, ty64, "cast", context.currentBlock());		// Cast index to 64 bit type
//		const Type* ty = Type::getIntNTy(TheContext,to.arraySize.getLimitedValue());
//		Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,ty,false);
//		Instruction* truncExt = CastInst::Create(op,exprResult,ty,"cast",context.currentBlock());		// Cast index to 64 bit type

		std::vector<llvm::Value*> indices;
		llvm::ConstantInt* index0 = context.getConstantZero(to.size.getLimitedValue());
		indices.push_back(index0);
		indices.push_back(truncExt);
		llvm::Instruction* elementPtr = llvm::GetElementPtrInst::Create(nullptr, to.value, indices, "array index", context.currentBlock());

		assignTo = elementPtr;//new LoadInst(elementPtr, "", false, context.currentBlock());
	}

	if (!assignTo->getType()->isPointerTy())
	{
		PrintErrorFromLocation(to.refLoc, "You cannot assign a value to a non variable (or input parameter to function)");
		context.errorFlagged = true;
		return nullptr;
	}

	// Handle variable promotion
	llvm::Type* ty = context.getIntType(to.size);
	llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(from, false, ty, false);

	llvm::Instruction* truncExt = llvm::CastInst::Create(op, from, ty, "cast", context.currentBlock());

	// Handle masking and constants and shift
	llvm::ConstantInt* const_intShift = context.getConstantInt(to.shft);
	llvm::BinaryOperator* shiftInst = llvm::BinaryOperator::Create(llvm::Instruction::Shl, truncExt, const_intShift, "Shifting", context.currentBlock());
	llvm::ConstantInt* const_intMask = context.getConstantInt(to.mask);
	llvm::BinaryOperator* andInst = llvm::BinaryOperator::Create(llvm::Instruction::And, shiftInst, const_intMask, "Masking", context.currentBlock());

	// cnst initialiser only used when we are updating the primary register
	llvm::BinaryOperator* final;

	if (to.aliased == false)
	{
		llvm::ConstantInt* const_intCnst = context.getConstantInt(to.cnst);
		llvm::BinaryOperator* orInst = llvm::BinaryOperator::Create(llvm::Instruction::Or, andInst, const_intCnst, "Constants", context.currentBlock());
		final = orInst;
	}
	else
	{
		llvm::Value* dest = to.value;
		if (toIdent.IsArray())
		{
			llvm::Value* exprResult = toIdent.GetExpression()->codeGen(context);

			llvm::Type* ty = context.getIntType(to.arraySize);
			llvm::Type* ty64 = context.getIntType(64);
			llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(exprResult, false, ty, false);
			llvm::Instruction* truncExt0 = llvm::CastInst::Create(op, exprResult, ty, "cast", context.currentBlock());		// Cast index to 64 bit type
			llvm::Instruction::CastOps op1 = llvm::CastInst::getCastOpcode(exprResult, false, ty64, false);
			llvm::Instruction* truncExt = llvm::CastInst::Create(op1, truncExt0, ty64, "cast", context.currentBlock());		// Cast index to 64 bit type

			std::vector<llvm::Value*> indices;
			llvm::ConstantInt* index0 = context.getConstantZero(to.size.getLimitedValue());
			indices.push_back(index0);
			indices.push_back(truncExt);
			llvm::Instruction* elementPtr = llvm::GetElementPtrInst::Create(nullptr, to.value, indices, "array index", context.currentBlock());

			dest = elementPtr;
		}
		// Now if the assignment is assigning to an aliased register part, we need to have loaded the original register, masked off the inverse of the section mask, and or'd in the result before we store
		llvm::LoadInst* loadInst = new llvm::LoadInst(dest, "", false, context.currentBlock());
		llvm::ConstantInt* const_intInvMask = context.getConstantInt(~to.mask);
		llvm::BinaryOperator* primaryAndInst = llvm::BinaryOperator::Create(llvm::Instruction::And, loadInst, const_intInvMask, "InvMasking", context.currentBlock());
		final = llvm::BinaryOperator::Create(llvm::Instruction::Or, primaryAndInst, andInst, "Combining", context.currentBlock());
	}

	if (to.fromExternal)
	{
		if (to.pinType != TOK_IN && to.pinType != TOK_BIDIRECTIONAL)
		{
			PrintErrorFromLocation(to.refLoc, "Pin marked as not writable");
			context.errorFlagged = true;
			return nullptr;
		}
		else
		{
			// At this point, we need to get PinSet method and call it.
			llvm::Function* function = context.LookupFunctionInExternalModule(toIdent.module, context.getSymbolPrefix() + "PinSet" + toIdent.name);

			std::vector<llvm::Value*> args;
			args.push_back(final);
			llvm::Instruction* I = llvm::CallInst::Create(function, args, "", context.currentBlock());
			if (context.gContext.opts.generateDebug)
			{
				I->setDebugLoc(llvm::DebugLoc::get(to.refLoc.first_line, to.refLoc.first_column, context.gContext.scopingStack.top()));
			}
			return I;
		}

	}
	else
	{
		if (to.impedance && clearImpedance)	// clear impedance
		{
			llvm::ConstantInt* impedance = context.getConstantZero(to.size.getLimitedValue());
			new llvm::StoreInst(impedance, to.impedance, false, context.currentBlock());
		}
		return new llvm::StoreInst(final, assignTo, isVolatile, context.currentBlock());
	}
}

void CAssignment::prePass(CodeGenContext& context)
{
	rhs.prePass(context);

	if (rhs.IsImpedance())
	{
		// We have a variable being assigned a high_impedance, for compatabilty with old (non bus connected modules), we only create impedance if we find at least one assignment
		assert(lhs.module == "");
		std::string fullName = context.moduleName + context.getSymbolPrefix() + lhs.name;
		if (context.gContext.impedanceRequired.find(fullName) == context.gContext.impedanceRequired.end())
		{
			context.gContext.impedanceRequired[fullName] = true;
		}
	}
}

llvm::Value* CAssignment::codeGen(CodeGenContext& context)
{
	BitVariable var;
	llvm::Value* assignWith;
	if (!context.LookupBitVariable(var, lhs.module, lhs.name, lhs.modLoc, lhs.nameLoc))
	{
		return nullptr;
	}

	if (var.mappingRef)
	{
		PrintErrorFromLocation(var.refLoc, "Cannot assign to a mapping reference");
		context.errorFlagged = true;
		return nullptr;
	}

	if (rhs.IsImpedance())
	{
		// special case, impedance uses hidden bit variable (which is only accessed during bus muxing)	- Going to need a few fixups to support this
		if (var.pinType != TOK_BIDIRECTIONAL)
		{
			PrintErrorFromLocation(var.refLoc, "HIGH_IMPEDANCE only makes sense for BIDIRECTIONAL pins");
			context.errorFlagged = true;
			return nullptr;
		}

		llvm::Instruction* I = CAssignment::generateImpedanceAssignment(var, var.impedance, context);
		if (context.gContext.opts.generateDebug)
		{
			assert(context.gContext.scopingStack.size() > 0);
			I->setDebugLoc(llvm::DebugLoc::get(operatorLoc.first_line, operatorLoc.first_column, context.gContext.scopingStack.top()));
		}
		return I;
	}
	assignWith = rhs.codeGen(context);
	if (assignWith == nullptr)
	{
		return nullptr;
	}

	llvm::Instruction* I = CAssignment::generateAssignment(var, lhs, assignWith, context);
	if (context.gContext.opts.generateDebug)
	{
		assert(context.gContext.scopingStack.size() > 0);
		I->setDebugLoc(llvm::DebugLoc::get(operatorLoc.first_line, operatorLoc.first_column, context.gContext.scopingStack.top()));
	}
	return I;
}

llvm::Value* CAssignment::codeGen(CodeGenContext& context, CCastOperator* cast)
{
	BitVariable var;
	llvm::Value* assignWith;
	if (!context.LookupBitVariable(var, lhs.module, lhs.name, lhs.modLoc, lhs.nameLoc))
	{
		return nullptr;
	}

	if (var.mappingRef)
	{
		PrintErrorFromLocation(var.refLoc, "Cannot assign to a mapping reference");
		context.errorFlagged = true;
		return nullptr;
	}

	var.aliased = true;		// We pretend we are assigning to an alias, even if we are not, this forces the compiler to generate the correct code

	llvm::APInt mask(var.size.getLimitedValue(), "0", 10);
	llvm::APInt start = cast->beg.getAPInt().zextOrTrunc(var.size.getLimitedValue());
	llvm::APInt loop = cast->beg.getAPInt().zextOrTrunc(var.size.getLimitedValue());
	llvm::APInt endloop = cast->end.getAPInt().zextOrTrunc(var.size.getLimitedValue());

	while (1 == 1)
	{
		mask.setBit(loop.getLimitedValue());
		if (loop.uge(endloop))
			break;
		loop++;
	}


	var.shft = start;
	var.mask = mask;

	assignWith = rhs.codeGen(context);

	return CAssignment::generateAssignment(var, lhs, assignWith, context);
}
