#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away
extern void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...);			// Todo refactor away

CInteger CVariableDeclaration::notArray("0");

void CVariableDeclaration::CreateWriteAccessor(CodeGenContext& context, BitVariable& var, const std::string& moduleName, const std::string& name, bool impedance)
{
	std::vector<llvm::Type*> argTypes;
	argTypes.push_back(llvm::IntegerType::get(TheContext, var.size.getLimitedValue()));
	llvm::FunctionType *ftype = llvm::FunctionType::get(llvm::Type::getVoidTy(TheContext), argTypes, false);
	llvm::Function* function;
	if (context.isRoot)
	{
		function = llvm::Function::Create(ftype, llvm::GlobalValue::ExternalLinkage, context.moduleName + context.symbolPrepend + "PinSet" + id.name, context.module);
	}
	else
	{
		function = llvm::Function::Create(ftype, llvm::GlobalValue::PrivateLinkage, context.moduleName + context.symbolPrepend + "PinSet" + id.name, context.module);
	}
	function->onlyReadsMemory();	// Mark input read only
	function->setDoesNotThrow();

	context.StartFunctionDebugInfo(function, declarationLoc);

	llvm::BasicBlock *bblock = llvm::BasicBlock::Create(TheContext, "entry", function, 0);

	context.pushBlock(bblock, declarationLoc);

	llvm::Function::arg_iterator args = function->arg_begin();
	llvm::Value* setVal = &*args;
	setVal->setName("InputVal");

	llvm::LoadInst* load = new llvm::LoadInst(var.value, "", false, bblock);

	if (impedance)
	{
		llvm::LoadInst* loadImp = new llvm::LoadInst(var.impedance, "", false, bblock);
		llvm::CmpInst* check = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_NE, loadImp, llvm::ConstantInt::get(TheContext, llvm::APInt(var.size.getLimitedValue(), 0)), "impedance", bblock);

		setVal = llvm::SelectInst::Create(check, setVal, load, "impOrReal", bblock);
	}

	llvm::Value* stor = CAssignment::generateAssignmentActual(var, id/*moduleName, name*/, setVal, context, false);		// we shouldn't clear impedance on write via pin (instead should ignore write if impedance is set)

	var.priorValue = load;
	var.writeInput = setVal;
	var.writeAccessor = &writeAccessor;
	writeAccessor = llvm::ReturnInst::Create(TheContext, bblock);

	context.popBlock(declarationLoc);

	context.EndFunctionDebugInfo();
}

void CVariableDeclaration::CreateReadAccessor(CodeGenContext& context, BitVariable& var, bool impedance)
{
	std::vector<llvm::Type*> argTypes;
	llvm::FunctionType *ftype = llvm::FunctionType::get(llvm::IntegerType::get(TheContext, var.size.getLimitedValue()), argTypes, false);
	llvm::Function* function;
	if (context.isRoot)
	{
		function = llvm::Function::Create(ftype, llvm::GlobalValue::ExternalLinkage, context.moduleName + context.symbolPrepend + "PinGet" + id.name, context.module);
	}
	else
	{
		function = llvm::Function::Create(ftype, llvm::GlobalValue::PrivateLinkage, context.moduleName + context.symbolPrepend + "PinGet" + id.name, context.module);
	}
	function->setOnlyReadsMemory();
	function->setDoesNotThrow();

	context.StartFunctionDebugInfo(function, declarationLoc);

	llvm::BasicBlock *bblock = llvm::BasicBlock::Create(TheContext, "entry", function, 0);

	context.pushBlock(bblock, declarationLoc);

	llvm::Value* load = new llvm::LoadInst(var.value, "", false, bblock);
	if (impedance)
	{
		llvm::LoadInst* loadImp = new llvm::LoadInst(var.impedance, "", false, bblock);
		llvm::CmpInst* check = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_EQ, loadImp, llvm::ConstantInt::get(TheContext, llvm::APInt(var.size.getLimitedValue(), 0)), "impedance", bblock);

		load = llvm::SelectInst::Create(check, load, loadImp, "impOrReal", bblock);
	}

	llvm::ReturnInst::Create(TheContext, load, bblock);

	context.popBlock(declarationLoc);

	context.EndFunctionDebugInfo();
}

void CVariableDeclaration::prePass(CodeGenContext& context)
{
	// Do nothing, but in future we could pre compute the globals and the locals for each block, allowing use before declaration?
}

llvm::Value* CVariableDeclaration::codeGen(CodeGenContext& context)
{
	BitVariable temp(size.integer, 0);

	temp.arraySize = arraySize.integer;
	temp.mask = ~temp.cnst;
	temp.pinType = pinType;

	if (context.currentBlock())
	{
		if (pinType != 0)
		{
			PrintErrorFromLocation(declarationLoc, "Cannot declare pins anywhere but global scope");
			context.errorFlagged = true;
			return nullptr;
		}
		// Within a basic block - so must be a stack variable
		llvm::AllocaInst *alloc = new llvm::AllocaInst(llvm::Type::getIntNTy(TheContext, size.integer.getLimitedValue()), 0, id.name.c_str(), context.currentBlock());
		temp.value = alloc;
	}
	else
	{
		// GLobal variable
		// Rules for globals have changed. If we are definining a PIN then the variable should be private to this module, and accessors should be created instead. 
		if (pinType == 0)
		{
			llvm::Type* type = llvm::Type::getIntNTy(TheContext, size.integer.getLimitedValue());
			if (arraySize.integer.getLimitedValue())
			{
				llvm::APInt power2(arraySize.integer.getLimitedValue() + 1, 1);
				power2 <<= arraySize.integer.getLimitedValue();
				type = llvm::ArrayType::get(llvm::Type::getIntNTy(TheContext, size.integer.getLimitedValue()), power2.getLimitedValue());
			}

			if (internal /*|| !context.isRoot*/)
			{
				temp.value = new llvm::GlobalVariable(*context.module, type, false, llvm::GlobalValue::PrivateLinkage, nullptr, context.symbolPrepend + id.name);
			}
			else
			{
				temp.value = new llvm::GlobalVariable(*context.module, type, false, llvm::GlobalValue::ExternalLinkage, nullptr, context.symbolPrepend + id.name);
			}
		}
		else
		{
			temp.value = new llvm::GlobalVariable(*context.module, llvm::Type::getIntNTy(TheContext, size.integer.getLimitedValue()), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.symbolPrepend + id.name);
			switch (pinType)
			{
			case TOK_IN:
				CreateWriteAccessor(context, temp, id.module, id.name, false);
				break;
			case TOK_OUT:
				CreateReadAccessor(context, temp, false);
				break;
			case TOK_BIDIRECTIONAL:
			{
				// we need a new variable to hold the impedance mask
				bool needsImpedance = false;
				if (context.gContext.impedanceRequired.find(context.moduleName + context.symbolPrepend + id.name) != context.gContext.impedanceRequired.end())
				{
					temp.impedance = new llvm::GlobalVariable(*context.module, llvm::Type::getIntNTy(TheContext, size.integer.getLimitedValue()), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.symbolPrepend + id.name + ".HZ");
					needsImpedance = true;
				}
				CreateWriteAccessor(context, temp, id.module, id.name, needsImpedance);
				CreateReadAccessor(context, temp, needsImpedance);
			}
			break;
			default:
				assert(0 && "Unhandled pin type");
				break;
			}
		}

	}

	llvm::APInt bitPos = size.integer - 1;
	for (int a = 0; a < aliases.size(); a++)
	{
		if (aliases[a]->idOrEmpty.name.size() < 1)
		{
			// For constants we need to update the mask and cnst values (we also need to set the initialiser properly - that will come later)

			llvm::APInt newMask = ~llvm::APInt(aliases[a]->sizeOrValue.integer.getBitWidth(), 0);
			llvm::APInt newCnst = aliases[a]->sizeOrValue.integer;

			bitPos -= llvm::APInt(bitPos.getBitWidth(), aliases[a]->sizeOrValue.integer.getBitWidth() - 1);

			if (newMask.getBitWidth() != size.integer.getLimitedValue())
			{
				newMask = newMask.zext(size.integer.getLimitedValue());
			}
			if (newCnst.getBitWidth() != size.integer.getLimitedValue())
			{
				newCnst = newCnst.zext(size.integer.getLimitedValue());
			}
			newCnst <<= bitPos.getLimitedValue();
			newMask <<= bitPos.getLimitedValue();
			temp.mask &= ~newMask;
			temp.cnst |= newCnst;

			bitPos -= llvm::APInt(bitPos.getBitWidth(), 1);
		}
		else
		{
			// For identifiers, we need to create another entry in the correct symbol table and set up the masks appropriately
			// e.g. BOB[4] ALIAS CAT[2]:%01
			// would create CAT with a shift of 2 a mask of %1100 (cnst will always be 0 for these)

			bitPos -= llvm::APInt(bitPos.getBitWidth(), aliases[a]->sizeOrValue.integer.getLimitedValue() - 1);

			BitVariable alias;
			alias.arraySize = temp.arraySize;
			alias.size = temp.size;
			alias.trueSize = aliases[a]->sizeOrValue.integer;
			alias.value = temp.value;	// the value will always point at the stored local/global
			alias.cnst = temp.cnst;		// ignored
			alias.mappingRef = false;
			alias.pinType = temp.pinType;
			alias.writeAccessor = temp.writeAccessor;
			alias.writeInput = temp.writeInput;
			alias.priorValue = temp.priorValue;
			alias.fromExternal = false;
			alias.impedance = nullptr;

			llvm::APInt newMask = ~llvm::APInt(aliases[a]->sizeOrValue.integer.getLimitedValue(), 0);
			if (newMask.getBitWidth() != size.integer.getLimitedValue())
			{
				newMask = newMask.zext(size.integer.getLimitedValue());
			}
			newMask <<= bitPos.getLimitedValue();
			alias.mask = newMask;		// need to generate correct mask

			alias.shft = llvm::APInt(size.integer.getLimitedValue(), bitPos.getLimitedValue());
			alias.aliased = true;

			if (context.currentBlock())
			{
				context.locals()[aliases[a]->idOrEmpty.name] = alias;
			}
			else
			{
				context.globals()[aliases[a]->idOrEmpty.name] = alias;
			}

			bitPos -= llvm::APInt(bitPos.getBitWidth(), 1);
		}
	}

	llvm::ConstantInt* const_intn_0 = llvm::ConstantInt::get(TheContext, temp.cnst);
	if (context.currentBlock())
	{
		// Initialiser Definitions for local scope (arrays are not supported at present)

		if (initialiserList.empty())
		{
			llvm::StoreInst* stor = new llvm::StoreInst(const_intn_0, temp.value, false, context.currentBlock());		// NOT SURE HOW WELL THIS WILL WORK IN FUTURE
		}
		else
		{
			if (initialiserList.size() != 1)
			{
				PrintErrorWholeLine(declarationLoc, "Too many initialisers (note array initialisers not currently supported in local scope)");
				context.errorFlagged = true;
				return nullptr;
			}

			llvm::APInt t = initialiserList[0]->integer;

			if (t.getBitWidth() != size.integer.getLimitedValue())
			{
				t = t.zext(size.integer.getLimitedValue());
			}

			llvm::ConstantInt* constInit = llvm::ConstantInt::get(TheContext, t);

			llvm::StoreInst* stor = new llvm::StoreInst(constInit, temp.value, false, context.currentBlock());
		}
		context.locals()[id.name] = temp;
	}
	else
	{
		// Initialiser Definitions for global scope
		if (temp.arraySize.getLimitedValue())
		{
			llvm::APInt power2(arraySize.integer.getLimitedValue() + 1, 1);
			power2 <<= arraySize.integer.getLimitedValue();

			if (initialiserList.empty())
			{
				llvm::ConstantAggregateZero* const_array_7 = llvm::ConstantAggregateZero::get(llvm::ArrayType::get(llvm::Type::getIntNTy(TheContext, size.integer.getLimitedValue()), power2.getLimitedValue()));
				llvm::cast<llvm::GlobalVariable>(temp.value)->setInitializer(const_array_7);
			}
			else
			{
				if (initialiserList.size() > power2.getLimitedValue())
				{
					PrintErrorWholeLine(declarationLoc, "Too many initialisers, initialiser list bigger than array storage");
					context.errorFlagged = true;
					return nullptr;
				}

				int a;
				std::vector<llvm::Constant*> const_array_9_elems;
				llvm::ArrayType* arrayTy = llvm::ArrayType::get(llvm::Type::getIntNTy(TheContext, size.integer.getLimitedValue()), power2.getLimitedValue());
				llvm::ConstantInt* const0 = llvm::ConstantInt::get(TheContext, llvm::APInt(size.integer.getLimitedValue(), 0));

				for (a = 0; a < power2.getLimitedValue(); a++)
				{
					if (a < initialiserList.size())
					{
						llvm::APInt t = initialiserList[a]->integer;

						if (t.getBitWidth() != size.integer.getLimitedValue())
						{
							t = t.zext(size.integer.getLimitedValue());
						}

						const_array_9_elems.push_back(llvm::ConstantInt::get(TheContext, t));
					}
					else
					{
						const_array_9_elems.push_back(const0);
					}
				}

				llvm::Constant* const_array_9 = llvm::ConstantArray::get(arrayTy, const_array_9_elems);
				llvm::cast<llvm::GlobalVariable>(temp.value)->setInitializer(const_array_9);
			}
		}
		else
		{
			if (initialiserList.empty())
			{
				llvm::cast<llvm::GlobalVariable>(temp.value)->setInitializer(const_intn_0);
				if (temp.impedance)
				{
					llvm::cast<llvm::GlobalVariable>(temp.impedance)->setInitializer(const_intn_0);
				}
			}
			else
			{
				if (initialiserList.size() != 1)
				{
					PrintErrorWholeLine(declarationLoc, "Too many initialisers");
					context.errorFlagged = true;
					return nullptr;
				}

				llvm::APInt t = initialiserList[0]->integer;

				if (t.getBitWidth() != size.integer.getLimitedValue())
				{
					t = t.zext(size.integer.getLimitedValue());
				}

				llvm::ConstantInt* constInit = llvm::ConstantInt::get(TheContext, t);
				llvm::cast<llvm::GlobalVariable>(temp.value)->setInitializer(constInit);
			}
		}

		context.globals()[id.name] = temp;
	}

	return temp.value;
}
