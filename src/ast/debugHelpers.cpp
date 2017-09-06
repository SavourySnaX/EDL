#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

llvm::Value* CDebugTraceString::codeGen(CodeGenContext& context)
{
	for (unsigned a = 1; a < string.quoted.length() - 1; a++)
	{
		std::vector<llvm::Value*> args;

		args.push_back(llvm::ConstantInt::get(TheContext, llvm::APInt(32, string.quoted[a])));
		llvm::CallInst *call = llvm::CallInst::Create(context.debugTraceChar, args, "DEBUGTRACE", context.currentBlock());
	}
	return nullptr;
}

llvm::Value* CDebugTraceInteger::codeGen(CodeGenContext& context)
{
	static char tbl[37] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	llvm::CallInst* fcall = nullptr;
	unsigned bitWidth = integer.integer.getBitWidth();
	// compute max divisor for associated base and bit width
	int cnt = 1;

	llvm::APInt baseDivisor(bitWidth * 2, currentBase);
	llvm::APInt oldBaseDivisor = baseDivisor;
	while (baseDivisor.getActiveBits() - 1 < bitWidth)
	{
		oldBaseDivisor = baseDivisor;
		baseDivisor *= llvm::APInt(bitWidth * 2, currentBase);

		cnt++;
	}

	baseDivisor = oldBaseDivisor.trunc(bitWidth);
	if (baseDivisor.getLimitedValue() == 0)
	{
		baseDivisor = llvm::APInt(bitWidth, 1);
	}

	for (unsigned a = 0; a < cnt; a++)
	{
		std::vector<llvm::Value*> args;

		llvm::APInt tmp = integer.integer.udiv(baseDivisor);

		args.push_back(llvm::ConstantInt::get(TheContext, llvm::APInt(32, tbl[tmp.getLimitedValue()])));
		llvm::CallInst *call = llvm::CallInst::Create(context.debugTraceChar, args, "DEBUGTRACE", context.currentBlock());

		integer.integer -= tmp*baseDivisor;
		if (cnt != 1)
		{
			baseDivisor = baseDivisor.udiv(llvm::APInt(bitWidth, currentBase));
		}
	}
	return fcall;
}

// The below provides the output for a single expr, will be called multiple times for arrays (at least for now)
llvm::Value* CDebugTraceIdentifier::generate(CodeGenContext& context, llvm::Value *loadedValue)
{
	const llvm::IntegerType* valueType = llvm::cast<llvm::IntegerType>(loadedValue->getType());
	unsigned bitWidth = valueType->getBitWidth();

	// compute max divisor for associated base and bit width
	int cnt = 1;

	llvm::APInt baseDivisor(bitWidth * 2, currentBase);
	llvm::APInt oldBaseDivisor = baseDivisor;
	while (baseDivisor.getActiveBits() - 1 < bitWidth)
	{
		oldBaseDivisor = baseDivisor;
		baseDivisor *= llvm::APInt(bitWidth * 2, currentBase);

		cnt++;
	}

	baseDivisor = oldBaseDivisor.trunc(bitWidth);
	if (baseDivisor.getLimitedValue() == 0)
	{
		baseDivisor = llvm::APInt(bitWidth, 1);
	}

	for (unsigned a = 0; a < cnt; a++)
	{
		std::vector<llvm::Value*> args;

		llvm::ConstantInt* const_intDiv = llvm::ConstantInt::get(TheContext, baseDivisor);
		llvm::BinaryOperator* shiftInst = llvm::BinaryOperator::Create(llvm::Instruction::UDiv, loadedValue, const_intDiv, "Dividing", context.currentBlock());

		llvm::Type* ty = llvm::Type::getIntNTy(TheContext, 32);
		llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(shiftInst, false, ty, false);
		llvm::Instruction* readyToAdd = llvm::CastInst::Create(op, shiftInst, ty, "cast", context.currentBlock());

		llvm::CmpInst* check = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_ULE, readyToAdd, llvm::ConstantInt::get(TheContext, llvm::APInt(32, 9)), "rangeCheck", context.currentBlock());

		llvm::BinaryOperator* lowAdd = llvm::BinaryOperator::Create(llvm::Instruction::Add, readyToAdd, llvm::ConstantInt::get(TheContext, llvm::APInt(32, '0')), "lowAdd", context.currentBlock());
		llvm::BinaryOperator* hiAdd = llvm::BinaryOperator::Create(llvm::Instruction::Add, readyToAdd, llvm::ConstantInt::get(TheContext, llvm::APInt(32, 'A' - 10)), "hiAdd", context.currentBlock());

		llvm::SelectInst *select = llvm::SelectInst::Create(check, lowAdd, hiAdd, "getRightChar", context.currentBlock());

		args.push_back(select);
		llvm::CallInst *call = llvm::CallInst::Create(context.debugTraceChar, args, "DEBUGTRACE", context.currentBlock());

		llvm::BinaryOperator* mulUp = llvm::BinaryOperator::Create(llvm::Instruction::Mul, shiftInst, const_intDiv, "mulUp", context.currentBlock());
		loadedValue = llvm::BinaryOperator::Create(llvm::Instruction::Sub, loadedValue, mulUp, "fixup", context.currentBlock());
		if (cnt != 1)
		{
			baseDivisor = baseDivisor.udiv(llvm::APInt(bitWidth, currentBase));
		}
	}
	return nullptr;
}

llvm::Value* CDebugTraceIdentifier::codeGen(CodeGenContext& context)
{
	int a;
	BitVariable var;

	if (!context.LookupBitVariable(var, ident.module, ident.name, ident.modLoc, ident.nameLoc))
	{
		return nullptr;
	}

	if (var.arraySize.getLimitedValue() == 0 || ident.IsArray())
	{
		std::string tmp = "\"";
		tmp += ident.name;
		tmp += "(\"";
		CString strng(tmp);
		CDebugTraceString identName(strng);

		identName.codeGen(context);

		llvm::Value* loadedValue = ident.codeGen(context);

		llvm::Value* ret = generate(context, loadedValue);

		std::vector<llvm::Value*> args;
		args.push_back(llvm::ConstantInt::get(TheContext, llvm::APInt(32, ')')));
		llvm::CallInst::Create(context.debugTraceChar, args, "DEBUGTRACE", context.currentBlock());

		return ret;
	}

	// presume they want the entire contents of array dumping

	{
		std::string tmp = "\"";
		tmp += ident.name;
		tmp += "(\"";
		CString strng(tmp);
		CDebugTraceString identName(strng);

		identName.codeGen(context);

		llvm::APInt power2(var.arraySize.getLimitedValue() + 1, 1);
		power2 <<= var.arraySize.getLimitedValue();
		for (a = 0; a < power2.getLimitedValue(); a++)
		{
			std::string tmp;
			tmp += llvm::Twine(a).str();
			CInteger index(tmp);
			CIdentifierArray idArrayTemp(index, ident.name);

			generate(context, idArrayTemp.codeGen(context));
			if (a != power2.getLimitedValue() - 1)
			{
				std::vector<llvm::Value*> args;
				args.push_back(llvm::ConstantInt::get(TheContext, llvm::APInt(32, ',')));
				llvm::CallInst::Create(context.debugTraceChar, args, "DEBUGTRACE", context.currentBlock());
			}
		}

		std::vector<llvm::Value*> args;
		args.push_back(llvm::ConstantInt::get(TheContext, llvm::APInt(32, ')')));
		llvm::CallInst::Create(context.debugTraceChar, args, "DEBUGTRACE", context.currentBlock());
	}

	return nullptr;
}


llvm::Value* CDebugLine::codeGen(CodeGenContext& context)		// Refactored away onto its own block - TODO factor away to a function - need to take care of passing identifiers forward though
{
	int currentBase = 2;

	llvm::BasicBlock *trac = llvm::BasicBlock::Create(TheContext, "debug_trace_helper", context.currentBlock()->getParent());
	llvm::BasicBlock *cont = llvm::BasicBlock::Create(TheContext, "continue", context.currentBlock()->getParent());

	llvm::BranchInst::Create(trac, context.currentBlock());

	context.pushBlock(trac, debugLocation);

	for (unsigned a = 0; a < debug.size(); a++)
	{
		if (debug[a]->isModifier())
		{
			currentBase = ((CDebugTraceBase*)debug[a])->integer.integer.getLimitedValue();
		}
		else
		{
			debug[a]->currentBase = currentBase;
			debug[a]->codeGen(context);
		}
	}

	std::vector<llvm::Value*> args;
	args.push_back(llvm::ConstantInt::get(TheContext, llvm::APInt(32, '\n')));
	llvm::CallInst* fcall = llvm::CallInst::Create(context.debugTraceChar, args, "DEBUGTRACE", context.currentBlock());

	llvm::BranchInst::Create(cont, context.currentBlock());

	context.popBlock(debugLocation);

	context.setBlock(cont);

	return nullptr;
}
