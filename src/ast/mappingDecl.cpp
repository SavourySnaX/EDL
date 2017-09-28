#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Value.h>

extern void PrintErrorDuplication(const YYLTYPE &location, const YYLTYPE &originalLocation, const char *errorstring, ...);		// Todo refactor away

void CMappingDeclaration::prePass(CodeGenContext& context)
{

}

llvm::Function* CMappingDeclaration::GenerateSetByMappingFunction(CodeGenContext& context,bool withCast)
{
	int a;
	BitVariable returnVal;
	std::vector<llvm::Type*> FuncTy_8_args;
	std::vector<llvm::BasicBlock*> caseBlocks;
	llvm::FunctionType* FuncTy_8;
	bool firstTime = true;
	llvm::APInt identSize;

	// Loop through mappings, make sure all mappings are identifiers, otherwise we cannot generate a setter function - Note at present array refs are not considered identifiers
	for (auto mapping : mappings)
	{
		if (!mapping->expr.IsIdentifierExpression())
		{
			return nullptr;							// Don't generate a setter for this mapping set, since we can't write to the expression
		}
		CIdentifier* identifier = (CIdentifier*)&mapping->expr;
		BitVariable mappingVariable;
		if (!context.LookupBitVariable(mappingVariable, identifier->module, identifier->name, identifier->modLoc, identifier->nameLoc))
		{
			return nullptr;
		}
		unsigned varSize = mappingVariable.trueSize.getLimitedValue();
		if (firstTime)
		{
			identSize = mappingVariable.trueSize;
			firstTime = false;
		}
		else
		{
			if (identSize.getLimitedValue() != varSize)
			{
				return context.gContext.ReportError(nullptr, EC_ErrorWholeLine, mapping->selector.getSourceLocation(), "Identifier must be same width for all expressions in a MAPPING");
			}
		}
	}

	// Input arguments for setByMapping are the selector size and the identifier size (computed above)
	FuncTy_8_args.push_back(context.getIntType(size));
	FuncTy_8_args.push_back(context.getIntType(identSize));
	if (withCast)
	{
		// extra arguments for the cast assignment are the constant shift and mask
		FuncTy_8_args.push_back(context.getIntType(identSize));
		FuncTy_8_args.push_back(context.getIntType(identSize));
	}

	FuncTy_8 = llvm::FunctionType::get(context.getVoidType(), FuncTy_8_args, false);

	llvm::Function* func = nullptr;
	func = context.makeFunction(FuncTy_8, llvm::GlobalValue::PrivateLinkage, context.getSymbolPrefix() + "setByMapping" + (withCast?"Cast":"") + ident.name);

	context.StartFunctionDebugInfo(func, ident.nameLoc);

	llvm::BasicBlock *bblock = context.makeBasicBlock("entry", func);
	llvm::BasicBlock *bblockCont = context.makeBasicBlock("default", func);

	context.pushBlock(bblock, blockStart);

	llvm::Function::arg_iterator args = func->arg_begin();
	BitVariable idx(size.getAPInt(),0);
	BitVariable val(identSize,0);
	BitVariable shift(identSize,0);
	BitVariable mask(identSize,0);

	idx.value = &*args++;
	idx.value->setName(ident.name+"idx");
	context.locals()[ident.name+"idx"] = idx;
	val.value = &*args++;
	val.value->setName(ident.name+"val");
	context.locals()[ident.name+"val"] = val;
	if (withCast)
	{
		shift.value = &*args++;
		shift.value->setName(ident.name + "shift");
		context.locals()[ident.name + "shift"] = shift;
		mask.value = &*args;
		mask.value->setName(ident.name + "mask");
		context.locals()[ident.name + "mask"] = mask;
	}

	llvm::SwitchInst* switchI=llvm::SwitchInst::Create(idx.value, bblockCont, 2<<size.getAPInt().getLimitedValue(), context.currentBlock());

	context.popBlock(blockEnd);

	// First step compute the case blocks, so we can examine the return types
	for (auto mapping : mappings)
	{
		llvm::BasicBlock *caseBlock = context.makeBasicBlock("case"+llvm::Twine(mapping->selector.getAPInt().getLimitedValue()).str(), func);

		switchI->addCase(context.getConstantInt(mapping->selector.getAPInt()), caseBlock);

		context.pushBlock(caseBlock, mapping->selector.getSourceLocation());

		CIdentifier* identifier = (CIdentifier*)&mapping->expr;
		BitVariable mappingVariable;
		if (!context.LookupBitVariable(mappingVariable, identifier->module, identifier->name, identifier->modLoc, identifier->nameLoc))
		{
			return nullptr;
		}

		llvm::Value* assignWith = val.value;
		if (withCast)
		{
			// Load value from dst, and combine with value coming from assignWith
			llvm::Value* origValue = identifier->codeGen(context);
			llvm::Value* invMask = llvm::BinaryOperator::CreateNot(mask.value,"InvMask",context.currentBlock());
			llvm::BinaryOperator* andInst = llvm::BinaryOperator::Create(llvm::Instruction::And, origValue, invMask, "MaskingOrig", context.currentBlock());
			llvm::BinaryOperator* shfSrc = llvm::BinaryOperator::Create(llvm::Instruction::Shl, assignWith, shift.value, "shiftInput", context.currentBlock());
			llvm::BinaryOperator* mskSrc = llvm::BinaryOperator::Create(llvm::Instruction::And, shfSrc, mask.value, "MaskInput", context.currentBlock());
			assignWith = llvm::BinaryOperator::Create(llvm::Instruction::Or, andInst, mskSrc, "Combine", context.currentBlock());
		}
		CAssignment::generateAssignment(mappingVariable, *identifier, assignWith, context);

		context.makeReturn(context.currentBlock());

		context.popBlock(mapping->selector.getSourceLocation());
	}

	context.pushBlock(bblockCont,blockStart);

	// default case
	context.makeReturn(bblockCont);

	context.popBlock(blockEnd);

	context.EndFunctionDebugInfo();

	return func;
}


llvm::Function* CMappingDeclaration::GenerateGetByMappingFunction(CodeGenContext& context, bool asVoid)
{
	int a;
	BitVariable returnVal;
	std::vector<llvm::Type*> FuncTy_8_args;
	llvm::FunctionType* FuncTy_8;
	bool firstMapping = true;
	unsigned returnWidth = 0;

	// Input argument for getByMapping is the selector size
	FuncTy_8_args.push_back(context.getIntType(size));

	// First step compute the case blocks, so we can examine the return types - only done for non void return
	for (auto mapping : mappings)
	{
		llvm::BasicBlock *caseBlock = context.makeBasicBlock("case"+llvm::Twine(mapping->selector.getAPInt().getLimitedValue()).str(), nullptr);

		context.pushBlock(caseBlock, mapping->selector.getSourceLocation());
		llvm::Value* mapResult = mapping->expr.codeGen(context);
		llvm::ReturnInst* mapReturn = nullptr;
		if (!asVoid)
		{
			mapReturn = context.makeReturnValue(mapResult, context.currentBlock());
		}
		else
		{
			mapReturn = context.makeReturn(context.currentBlock());
		}
		context.popBlock(mapping->selector.getSourceLocation());

		if (!asVoid)
		{
			// Get size of mapReturn
			if (mapReturn->getReturnValue()==nullptr || !mapReturn->getReturnValue()->getType()->isIntegerTy())
			{
				delete caseBlock;
				return nullptr;		// Does not return a value, so not suitable for getter
			}

			unsigned retBitWidth = mapReturn->getReturnValue()->getType()->getIntegerBitWidth();
			if (firstMapping)
			{
				returnWidth = retBitWidth;
				firstMapping = false;
			}
			else
			{
				if (returnWidth != retBitWidth)
				{
					delete caseBlock;
					return context.gContext.ReportError(nullptr, EC_ErrorWholeLine, mapping->selector.getSourceLocation(), "Mapping result must be same width for all expressions in a MAPPING");
				}
			}
		}
		delete caseBlock;
	}

	if (!asVoid)
	{
		FuncTy_8 = llvm::FunctionType::get(context.getIntType(returnWidth), FuncTy_8_args, false);
	}
	else
	{
		FuncTy_8 = llvm::FunctionType::get(context.getVoidType(), FuncTy_8_args, false);
	}

	llvm::Function* func = nullptr;
	func = context.makeFunction(FuncTy_8, llvm::GlobalValue::PrivateLinkage, context.getSymbolPrefix() + (!asVoid?"get":"do") + "ByMapping" + ident.name);

	context.StartFunctionDebugInfo(func, ident.nameLoc);

	llvm::BasicBlock *bblock = context.makeBasicBlock("entry", func);
	llvm::BasicBlock *bblockCont = context.makeBasicBlock("default", func);

	context.pushBlock(bblock, blockStart);

	llvm::Function::arg_iterator args = func->arg_begin();
	BitVariable temp(size.getAPInt(),0);

	temp.value = &*args;
	temp.value->setName(ident.name);
	context.locals()[ident.name] = temp;

	llvm::SwitchInst* switchI=llvm::SwitchInst::Create(temp.value, bblockCont, 2<<size.getAPInt().getLimitedValue(), context.currentBlock());

	context.popBlock(blockEnd);

	for (auto mapping : mappings)
	{
		llvm::BasicBlock *caseBlock = context.makeBasicBlock("case"+llvm::Twine(mapping->selector.getAPInt().getLimitedValue()).str(), func);

		context.pushBlock(caseBlock,mapping->selector.getSourceLocation());
		llvm::Value* mapResult = mapping->expr.codeGen(context);
		if (!asVoid)
		{
			context.makeReturnValue(mapResult, context.currentBlock());
		}
		else
		{
			context.makeReturn(context.currentBlock());
		}
		context.popBlock(mapping->selector.getSourceLocation());
		switchI->addCase(context.getConstantInt(mapping->selector.getAPInt()), caseBlock);
	}

	context.pushBlock(bblockCont,blockStart);

	// default case
	if (!asVoid)
	{
		context.makeReturnValue(context.getConstantZero(returnWidth), context.currentBlock());
	}
	else
	{
		context.makeReturn(context.currentBlock());
	}

	context.popBlock(blockEnd);

	context.EndFunctionDebugInfo();

	return func;
}


llvm::Value* CMappingDeclaration::codeGen(CodeGenContext& context)
{
	if (context.m_mappings.find(ident.name) != context.m_mappings.end())
	{
		return context.gContext.ReportDuplicationError(nullptr, ident.nameLoc, context.m_mappings[ident.name]->ident.nameLoc, "Multiple declaration for mapping");
	}
	else
	{
		context.m_mappings[ident.name] = this;

		getByMapping = GenerateGetByMappingFunction(context, false);
		doByMapping = GenerateGetByMappingFunction(context, true);
		setByMapping = GenerateSetByMappingFunction(context, false);
		setByMappingCast = GenerateSetByMappingFunction(context, true);
	}
	return nullptr;
}

llvm::Value* CMappingDeclaration::generateCallGetByMapping(CodeGenContext& context,const YYLTYPE& modLoc,const YYLTYPE& nameLoc)
{
	llvm::Function* getOrDo = getByMapping;
	if (getByMapping == nullptr)
	{
		getOrDo = doByMapping;
		if (doByMapping == nullptr)
		{
			return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, nameLoc, "Mapping is not referenceable (does not return values?)");
		}
	}

	std::vector<llvm::Value*> args;
	//get idx variable
	args.push_back(context.locals()[ident.name + "idx"].value);

	llvm::Instruction* call = llvm::CallInst::Create(getOrDo, args, "", context.currentBlock());
	if (context.gContext.opts.generateDebug)
	{
		call->setDebugLoc(llvm::DebugLoc::get(nameLoc.first_line, nameLoc.first_column, context.gContext.scopingStack.top()));
	}

	return call;
}

llvm::Value* CMappingDeclaration::generateCallSetByMapping(CodeGenContext& context, const YYLTYPE& modLoc, const YYLTYPE& nameLoc, llvm::Value* rhs)
{
	if (setByMapping == nullptr)
	{
		return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, nameLoc, "Mapping is not assignable (is not a simple identifer?)");
	}

	std::vector<llvm::Value*> args;
	//get idx variable
	args.push_back(context.locals()[ident.name + "idx"].value);
	args.push_back(rhs);

	llvm::Instruction* call = llvm::CallInst::Create(setByMapping, args, "", context.currentBlock());
	if (context.gContext.opts.generateDebug)
	{
		call->setDebugLoc(llvm::DebugLoc::get(nameLoc.first_line, nameLoc.first_column, context.gContext.scopingStack.top()));
	}

	return call;
}

llvm::Value* CMappingDeclaration::generateCallSetByMappingCast(CodeGenContext& context, const YYLTYPE& modLoc, const YYLTYPE& nameLoc, llvm::Value* rhs,CCastOperator* cast)
{
	if (setByMappingCast == nullptr)
	{
		return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, nameLoc, "Mapping is not assignable (is not a simple identifer?)");
	}

	llvm::FunctionType* fType = setByMappingCast->getFunctionType();
	llvm::Type* vType=fType->getParamType(1);	// get Type of second argument (which will give us the size needed for the shifts and masks)
	if (!vType->isIntegerTy())
	{
		return context.gContext.ReportError(nullptr, EC_InternalError, nameLoc, "Internal Error : Parameter to ByMappingCast not an integer type!");
	}
	unsigned bitWidth = vType->getIntegerBitWidth();
	std::vector<llvm::Value*> args;
	//get idx variable
	args.push_back(context.locals()[ident.name + "idx"].value);
	if (!rhs->getType()->isIntegerTy())
	{
		return context.gContext.ReportError(nullptr, EC_InternalError, nameLoc, "Internal Error : Argument to ByMappingCast not an integer type!");
	}
	unsigned rhsBitWidth = rhs->getType()->getIntegerBitWidth();
	if (rhsBitWidth > bitWidth)
	{
		return context.gContext.ReportError(nullptr, EC_InternalError, nameLoc, "TODO- can this occur?");
	}
	if (rhsBitWidth < bitWidth)
	{
		args.push_back(new llvm::ZExtInst(rhs, vType, "ZExt", context.currentBlock()));
	}
	else
	{
		args.push_back(rhs);
	}

	// Compute cast shift and mask
	llvm::APInt mask(bitWidth, "0", 10);
	llvm::APInt start = cast->beg.getAPInt().zextOrTrunc(bitWidth);
	llvm::APInt loop = cast->beg.getAPInt().zextOrTrunc(bitWidth);
	llvm::APInt endloop = cast->end.getAPInt().zextOrTrunc(bitWidth);

	while (1 == 1)
	{
		mask.setBit(loop.getLimitedValue());
		if (loop.uge(endloop))
			break;
		loop++;
	}

	args.push_back(context.getConstantInt(start));	// Shift
	args.push_back(context.getConstantInt(mask));	// Mask

	llvm::Instruction* call = llvm::CallInst::Create(setByMappingCast, args, "", context.currentBlock());
	if (context.gContext.opts.generateDebug)
	{
		call->setDebugLoc(llvm::DebugLoc::get(nameLoc.first_line, nameLoc.first_column, context.gContext.scopingStack.top()));
	}

	return call;
}
