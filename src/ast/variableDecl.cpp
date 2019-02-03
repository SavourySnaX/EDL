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

CInteger CVariableDeclaration::notArray("0");

void CVariableDeclaration::CreateWriteAccessor(CodeGenContext& context, BitVariable& var, const std::string& moduleName, const std::string& name, bool impedance)
{
	std::vector<llvm::Type*> argTypes;
	argTypes.push_back(context.getIntType(var.size));
	llvm::FunctionType *ftype = llvm::FunctionType::get(context.getVoidType(), argTypes, false);
	llvm::Function* function;
	if (context.isRoot)
	{
		function = context.makeFunction(ftype, llvm::GlobalValue::ExternalLinkage, context.moduleName + context.getSymbolPrefix() + "PinSet" + id.name);
	}
	else
	{
		function = context.makeFunction(ftype, llvm::GlobalValue::PrivateLinkage, context.moduleName + context.getSymbolPrefix() + "PinSet" + id.name);
	}
	function->onlyReadsMemory();	// Mark input read only

	context.StartFunctionDebugInfo(function, declarationLoc);

	llvm::BasicBlock *bblock = context.makeBasicBlock("entry", function);

	context.pushBlock(bblock, declarationLoc);

	llvm::Function::arg_iterator args = function->arg_begin();
	llvm::Value* setVal = &*args;
	setVal->setName("InputVal");

	llvm::LoadInst* load = new llvm::LoadInst(var.value, "", false, bblock);

	if (impedance)
	{
		llvm::LoadInst* loadImp = new llvm::LoadInst(var.impedance, "", false, bblock);
		llvm::CmpInst* check = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_NE, loadImp, context.getConstantZero(var.size.getLimitedValue()), "impedance", bblock);

		setVal = llvm::SelectInst::Create(check, setVal, load, "impOrReal", bblock);
	}

	llvm::Value* stor = CAssignment::generateAssignmentActual(var, id/*moduleName, name*/, setVal, context, false);		// we shouldn't clear impedance on write via pin (instead should ignore write if impedance is set)

	var.priorValue = load;
	var.writeInput = setVal;
	var.writeAccessor = &writeAccessor;
	writeAccessor = context.makeReturn(bblock);

	context.popBlock(declarationLoc);

	context.EndFunctionDebugInfo();
}

void CVariableDeclaration::CreateReadAccessor(CodeGenContext& context, BitVariable& var, bool impedance)
{
	std::vector<llvm::Type*> argTypes;
	llvm::FunctionType *ftype = llvm::FunctionType::get(context.getIntType(var.size), argTypes, false);
	llvm::Function* function;
	if (context.isRoot)
	{
		function = context.makeFunction(ftype, llvm::GlobalValue::ExternalLinkage, context.moduleName + context.getSymbolPrefix() + "PinGet" + id.name);
	}
	else
	{
		function = context.makeFunction(ftype, llvm::GlobalValue::PrivateLinkage, context.moduleName + context.getSymbolPrefix() + "PinGet" + id.name);
	}
	function->setOnlyReadsMemory();

	context.StartFunctionDebugInfo(function, declarationLoc);

	llvm::BasicBlock *bblock = context.makeBasicBlock("entry", function);

	context.pushBlock(bblock, declarationLoc);

	llvm::Value* load = new llvm::LoadInst(var.value, "", false, bblock);
	if (impedance)
	{
		llvm::LoadInst* loadImp = new llvm::LoadInst(var.impedance, "", false, bblock);
		llvm::CmpInst* check = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_EQ, loadImp, context.getConstantZero(var.size.getLimitedValue()), "impedance", bblock);

		load = llvm::SelectInst::Create(check, load, loadImp, "impOrReal", bblock);
	}

	context.makeReturnValue(load, bblock);

	context.popBlock(declarationLoc);

	context.EndFunctionDebugInfo();
}

void CVariableDeclaration::prePass(CodeGenContext& context)
{
	// Do nothing, but in future we could pre compute the globals and the locals for each block, allowing use before declaration?
}

llvm::Value* CVariableDeclaration::codeGen(CodeGenContext& context)
{
	BitVariable temp(size.getAPInt(), 0);

	temp.arraySize = arraySize.getAPInt();
	temp.pinType = pinType;
	temp.refLoc = declarationLoc;

	if (context.currentBlock())
	{
		if (pinType != 0)
		{
			return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, declarationLoc, "Cannot declare pins anywhere but global scope");
		}
		// Within a basic block - so must be a stack variable
		llvm::AllocaInst *alloc = new llvm::AllocaInst(context.getIntType(size), 0, id.name.c_str(), context.currentBlock());
		temp.value = alloc;
	}
	else
	{
		// GLobal variable
		// Rules for globals have changed. If we are defining a PIN then the variable should be private to this module, and accessors should be created instead. 
		if (pinType == 0)
		{
			llvm::Type* type = context.getIntType(size);
			if (arraySize.getAPInt().getLimitedValue())
			{
				llvm::APInt power2(arraySize.getAPInt().getLimitedValue() + 1, 1);
				power2 <<= arraySize.getAPInt().getLimitedValue();
				type = llvm::ArrayType::get(context.getIntType(size), power2.getLimitedValue());
			}

			if (internal)
			{
				temp.value = context.makeGlobal(type, false, llvm::GlobalValue::PrivateLinkage, nullptr, context.getSymbolPrefix() + id.name);
			}
			else
			{
				temp.value = context.makeGlobal(type, false, llvm::GlobalValue::ExternalLinkage, nullptr, context.getSymbolPrefix() + id.name);
			}
		}
		else
		{
			temp.value = context.makeGlobal(context.getIntType(size), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.getSymbolPrefix() + id.name);
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
				if (context.gContext.impedanceRequired.find(context.moduleName + context.getSymbolPrefix() + id.name) != context.gContext.impedanceRequired.end())
				{
					temp.impedance = context.makeGlobal(context.getIntType(size), false, llvm::GlobalValue::PrivateLinkage, nullptr, context.getSymbolPrefix() + id.name + ".HZ");
					needsImpedance = true;
				}
				CreateWriteAccessor(context, temp, id.module, id.name, needsImpedance);
				CreateReadAccessor(context, temp, needsImpedance);
			}
			break;
			default:
				return context.gContext.ReportError(nullptr, EC_InternalError, declarationLoc, "Unhandled pin type");
			}
		}

	}

	// Safety check, validate that all of the aliases and original variable match in size
	for (auto aliasset : aliases)
	{
		uint64_t bitCount = 0;
		for (auto alias : aliasset)
		{
			if (alias->size_not_value)
				bitCount += alias->sizeOrValue.getAPInt().getLimitedValue();
			else
				bitCount += alias->sizeOrValue.getAPInt().getBitWidth();
		}
		if (bitCount)
		{
			if (bitCount != size.getAPInt().getLimitedValue())
			{
				return context.gContext.ReportError(nullptr, EC_ErrorWholeLine, declarationLoc, "Alias bit count doesn't match parent bit count");
			}
		}
	}

	// Create our various aliases
	for (auto aliasset : aliases)
	{
		llvm::APInt bitPos = size.getAPInt() - 1;
		for (int a = 0; a < aliasset.size(); a++)
		{
			if (!aliasset[a]->size_not_value)
			{
				// For constants we need to update the mask and cnst values (we also need to set the initialiser properly - that will come later)

				llvm::APInt newMask = ~llvm::APInt(aliasset[a]->sizeOrValue.getAPInt().getBitWidth(), 0);
				llvm::APInt newCnst = aliasset[a]->sizeOrValue.getAPInt();

				bitPos -= llvm::APInt(bitPos.getBitWidth(), aliasset[a]->sizeOrValue.getAPInt().getBitWidth() - 1);

				if (newMask.getBitWidth() != size.getAPInt().getLimitedValue())
				{
					newMask = newMask.zext(size.getAPInt().getLimitedValue());
				}
				if (newCnst.getBitWidth() != size.getAPInt().getLimitedValue())
				{
					newCnst = newCnst.zext(size.getAPInt().getLimitedValue());
				}
				newCnst <<= bitPos.getLimitedValue();
				newMask <<= bitPos.getLimitedValue();
				temp.mask &= ~newMask;
				temp.cnst |= newCnst;

				bitPos -= llvm::APInt(bitPos.getBitWidth(), 1);
			}
			else
			{
				if (aliasset[a]->idOrEmpty.name.size() > 0)
				{
					// For identifiers, we need to create another entry in the correct symbol table and set up the masks appropriately
					// e.g. BOB[4] ALIAS CAT[2]:%01
					// would create CAT with a shift of 2 a mask of %1100 (cnst will always be 0 for these)

					bitPos -= llvm::APInt(bitPos.getBitWidth(), aliasset[a]->sizeOrValue.getAPInt().getLimitedValue() - 1);

					BitVariable alias;
					alias.arraySize = temp.arraySize;
					alias.size = temp.size;
					alias.trueSize = aliasset[a]->sizeOrValue.getAPInt();
					alias.value = temp.value;	// the value will always point at the stored local/global
					alias.cnst = temp.cnst;		// ignored
					alias.mappingRef = false;
					alias.pinType = temp.pinType;
					alias.writeAccessor = temp.writeAccessor;
					alias.writeInput = temp.writeInput;
					alias.priorValue = temp.priorValue;
					alias.fromExternal = false;
					alias.impedance = nullptr;
					alias.refLoc = declarationLoc;

					llvm::APInt newMask = ~llvm::APInt(aliasset[a]->sizeOrValue.getAPInt().getLimitedValue(), 0);
					if (newMask.getBitWidth() != size.getAPInt().getLimitedValue())
					{
						newMask = newMask.zext(size.getAPInt().getLimitedValue());
					}
					newMask <<= bitPos.getLimitedValue();
					alias.mask = newMask;		// need to generate correct mask

					alias.shft = llvm::APInt(size.getAPInt().getLimitedValue(), bitPos.getLimitedValue());
					alias.aliased = true;

					if (context.currentBlock())
					{
						if (context.locals().find(aliasset[a]->idOrEmpty.name) != context.locals().end())
						{
							return context.gContext.ReportDuplicationError(nullptr, declarationLoc, context.locals()[aliasset[a]->idOrEmpty.name].refLoc, "Duplicate '%s' (already locally defined)",aliasset[a]->idOrEmpty.name.c_str());
						}
						if (context.globals().find(aliasset[a]->idOrEmpty.name) != context.globals().end())
						{
							return context.gContext.ReportDuplicationError(nullptr, declarationLoc, context.globals()[aliasset[a]->idOrEmpty.name].refLoc, "Duplicate '%s' (shadows global variable declaration)",aliasset[a]->idOrEmpty.name.c_str());
						}
						context.locals()[aliasset[a]->idOrEmpty.name] = alias;
					}
					else
					{
						if (context.globals().find(aliasset[a]->idOrEmpty.name) != context.globals().end())
						{
							return context.gContext.ReportDuplicationError(nullptr, declarationLoc, context.globals()[aliasset[a]->idOrEmpty.name].refLoc, "Duplicate '%s' (shadows global variable declaration)",aliasset[a]->idOrEmpty.name.c_str());
						}
						context.globals()[aliasset[a]->idOrEmpty.name] = alias;
					}

					bitPos -= llvm::APInt(bitPos.getBitWidth(), 1);
				}
				else
				{
					// For an anonymous alias, just skip the bits
					bitPos -= llvm::APInt(bitPos.getBitWidth(), aliasset[a]->sizeOrValue.getAPInt().getLimitedValue() - 1);
					bitPos -= llvm::APInt(bitPos.getBitWidth(), 1);
				}
			}
		}
	}

	llvm::ConstantInt* const_intn_0 = context.getConstantInt(temp.cnst);
	if (context.currentBlock())
	{
		// Initialiser Definitions for local scope (arrays are not supported at present)
		if (context.locals().find(id.name) != context.locals().end())
		{
			return context.gContext.ReportDuplicationError(nullptr, declarationLoc, context.locals()[id.name].refLoc, "Duplicate'%s' (already locally defined)",id.name.c_str());
		}
		if (context.globals().find(id.name) != context.globals().end())
		{
			return context.gContext.ReportDuplicationError(nullptr, declarationLoc, context.globals()[id.name].refLoc, "Duplicate '%s' (shadows global variable declaration)",id.name.c_str());
		}

		if (initialiserList.empty())
		{
			llvm::StoreInst* stor = new llvm::StoreInst(const_intn_0, temp.value, false, context.currentBlock());		// NOT SURE HOW WELL THIS WILL WORK IN FUTURE
		}
		else
		{
			if (initialiserList.size() != 1)
			{
				return context.gContext.ReportError(nullptr, EC_ErrorWholeLine, declarationLoc, "Too many initialisers (note array initialisers not currently supported in local scope)");
			}

			llvm::APInt t = initialiserList[0]->getAPInt();

			if (t.getBitWidth() != size.getAPInt().getLimitedValue())
			{
				t = t.zext(size.getAPInt().getLimitedValue());
			}

			llvm::ConstantInt* constInit = context.getConstantInt(t);

			llvm::StoreInst* stor = new llvm::StoreInst(constInit, temp.value, false, context.currentBlock());
		}
		context.locals()[id.name] = temp;
	}
	else
	{
		if (context.globals().find(id.name) != context.globals().end())
		{
			return context.gContext.ReportDuplicationError(nullptr, declarationLoc, context.globals()[id.name].refLoc, "Duplicate '%s' (already globally defined)",id.name.c_str());
		}
		// Initialiser Definitions for global scope
		if (temp.arraySize.getLimitedValue())
		{
			llvm::APInt power2(arraySize.getAPInt().getLimitedValue() + 1, 1);
			power2 <<= arraySize.getAPInt().getLimitedValue();

			if (initialiserList.empty())
			{
				llvm::ConstantAggregateZero* const_array_7 = llvm::ConstantAggregateZero::get(llvm::ArrayType::get(context.getIntType(size), power2.getLimitedValue()));
				llvm::cast<llvm::GlobalVariable>(temp.value)->setInitializer(const_array_7);
			}
			else
			{
				if (initialiserList.size() > power2.getLimitedValue())
				{
					return context.gContext.ReportError(nullptr, EC_ErrorWholeLine, declarationLoc, "Too many initialisers, initialiser list bigger than array storage");
				}

				int a;
				std::vector<llvm::Constant*> const_array_9_elems;
				llvm::ArrayType* arrayTy = llvm::ArrayType::get(context.getIntType(size), power2.getLimitedValue());
				llvm::ConstantInt* const0 = context.getConstantZero(size.getAPInt().getLimitedValue());

				for (a = 0; a < power2.getLimitedValue(); a++)
				{
					if (a < initialiserList.size())
					{
						llvm::APInt t = initialiserList[a]->getAPInt();

						if (t.getBitWidth() != size.getAPInt().getLimitedValue())
						{
							t = t.zext(size.getAPInt().getLimitedValue());
						}

						const_array_9_elems.push_back(context.getConstantInt(t));
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
					return context.gContext.ReportError(nullptr, EC_ErrorWholeLine, declarationLoc, "Too many initialisers");
				}

				llvm::APInt t = initialiserList[0]->getAPInt();

				if (t.getBitWidth() != size.getAPInt().getLimitedValue())
				{
					t = t.zext(size.getAPInt().getLimitedValue());
				}

				llvm::ConstantInt* constInit = context.getConstantInt(t);
				llvm::cast<llvm::GlobalVariable>(temp.value)->setInitializer(constInit);
			}
		}

		context.globals()[id.name] = temp;
	}

	return temp.value;
}
