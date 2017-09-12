#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

llvm::Value* CIdentifier::trueSize(llvm::Value* in, CodeGenContext& context, BitVariable& var)
{
	if (var.mappingRef)
	{
		PrintErrorFromLocation(var.refLoc, "(TODO)Cannot perform operation on a mapping reference");
		context.FlagError();
		return nullptr;
	}

	llvm::Type* varType = context.getIntType(var.trueSize);
	llvm::Instruction::CastOps castOperation = llvm::CastInst::getCastOpcode(in, false, varType, false);

	return llvm::CastInst::Create(castOperation, in, varType, "matchInputSize", context.currentBlock());
}

llvm::Value* CIdentifier::GetAliasedData(CodeGenContext& context, llvm::Value* in, BitVariable& var)
{
	if (var.aliased == true)
	{
		// We are loading from a partial value - we need to load, mask off correct result and shift result down to correct range
		llvm::ConstantInt* constIntMask = context.getConstantInt(var.mask);
		llvm::BinaryOperator* andInst = llvm::BinaryOperator::Create(llvm::Instruction::And, in, constIntMask, "Masking", context.currentBlock());
		llvm::ConstantInt* constIntShift = context.getConstantInt(var.shft);
		llvm::BinaryOperator* shiftInst = llvm::BinaryOperator::Create(llvm::Instruction::LShr, andInst, constIntShift, "Shifting", context.currentBlock());
		return trueSize(shiftInst, context, var);
	}

	return in;
}

llvm::Value* CIdentifier::codeGen(CodeGenContext& context)
{
	BitVariable var;

	if (!context.LookupBitVariable(var,module,name,modLoc,nameLoc))
	{
		return nullptr;
	}

	if (var.mappingRef)
	{
		return var.mapping->codeGen(context);
	}
	
	if (var.fromExternal)
	{
		if (var.pinType!=TOK_OUT && var.pinType!=TOK_BIDIRECTIONAL)
		{
			PrintErrorFromLocation(var.refLoc,"Pin marked as not readable");
			context.FlagError();
			return nullptr;
		}
		else
		{
			// At this point, we need to get the PinGet method and call it.
			
			llvm::Function* pinGetFunction=context.LookupFunctionInExternalModule(module,context.getSymbolPrefix()+"PinGet"+name);

			std::vector<llvm::Value*> args;
			llvm::Instruction* I=llvm::CallInst::Create(pinGetFunction,args,"GetPinValue",context.currentBlock());
			if (context.gContext.opts.generateDebug)
			{
				I->setDebugLoc(llvm::DebugLoc::get(nameLoc.first_line, nameLoc.first_column, context.gContext.scopingStack.top()));
			}
			return I;
		}

	}

	if (var.value->getType()->isPointerTy())
	{
		llvm::Value* loadInstruction = nullptr;
		if (IsArray())
		{
			llvm::Value* index=GetExpression()->codeGen(context);

			llvm::Type* typeArraySize = context.getIntType(var.arraySize);
			llvm::Type* type64 = context.getIntType(64);
			llvm::Instruction::CastOps castOperationArraySize = llvm::CastInst::getCastOpcode(index,false,typeArraySize,false);
			llvm::Instruction* truncOrExtArraySize = llvm::CastInst::Create(castOperationArraySize,index,typeArraySize,"sizeToArraySize",context.currentBlock());	// Cast index to array size (may truncate)
			llvm::Instruction::CastOps castOperation64 = llvm::CastInst::getCastOpcode(index,false,type64,false);
			llvm::Instruction* truncOrExtTo64 = llvm::CastInst::Create(castOperation64,truncOrExtArraySize,type64,"cast",context.currentBlock());		// Cast index to 64 bit type for GEP

 			std::vector<llvm::Value*> indices;
 			llvm::ConstantInt* index0 = context.getConstantZero(var.size.getLimitedValue());
			indices.push_back(index0);
 			indices.push_back(truncOrExtTo64);
			llvm::Instruction* elementPtr = llvm::GetElementPtrInst::Create(nullptr,var.value,indices,"array index",context.currentBlock());

			loadInstruction = new llvm::LoadInst(elementPtr, "", false, context.currentBlock());
		}
		else
		{
			loadInstruction = new llvm::LoadInst(var.value, "", false, context.currentBlock());
		}
		return GetAliasedData(context,loadInstruction,var);
	}

	return var.value;
}

