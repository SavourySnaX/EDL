#include "yyltype.h"
#include "ast.h"
#include "generator.h"
#include "parser.hpp"
#include "optPasses.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>

CIdentifier CAliasDeclaration::empty("");

extern YYLTYPE CombineTokenLocations(const YYLTYPE &a, const YYLTYPE &b);

std::string SanitiseNameForDebug(llvm::StringRef inputName)
{
	std::string t = inputName.str();
	std::replace(t.begin(), t.end(), '.', '_');
	return t;
}

CodeGenContext::CodeGenContext(GlobalContext& globalContext,CodeGenContext* parent) : gContext(globalContext)
{
	moduleName="";
	isRoot = parent == nullptr;
}
    

bool CompareEquals(llvm::APInt a,llvm::APInt b)
{
	if (a.getBitWidth()!=b.getBitWidth())
	{
		if (a.getBitWidth()<b.getBitWidth())
			a=a.zext(b.getBitWidth());
		else
			b=b.zext(a.getBitWidth());
	}

	return a==b;
}

void CodeGenContext::StartFunctionDebugInfo(llvm::Function* func, const YYLTYPE& declLoc)
{
	if (gContext.opts.generateDebug)
	{
		llvm::Type* retType = func->getReturnType();

		// Create function type information

		llvm::SmallVector<llvm::Metadata *, 16> EltTys;

		// Add the result type.
		if (retType->isVoidTy())
			EltTys.push_back(nullptr);
		else if (retType->isIntegerTy())
		{
			std::string name = "Bit" + std::to_string(retType->getIntegerBitWidth());
			EltTys.push_back(gContext.dbgBuilder->createBasicType(name, retType->getIntegerBitWidth(), 0));
		}
		else
		{
			gContext.ReportError(nullptr, EC_InternalError, declLoc, "Internal Compiler error, return type is not void or integer");
			return;
		}
		for (const auto& arg : func->getFunctionType()->params())
		{
			if (!arg->isIntegerTy())
			{
				gContext.ReportError(nullptr, EC_InternalError, declLoc, "Internal Compiler error, arg type is not integer");
				return;
			}
			std::string name = "Bit" + std::to_string(arg->getIntegerBitWidth());
			EltTys.push_back(gContext.dbgBuilder->createBasicType(name, arg->getIntegerBitWidth(), 0));
		}

		llvm::DISubroutineType* dbgFuncTy = gContext.dbgBuilder->createSubroutineType(gContext.dbgBuilder->getOrCreateTypeArray(EltTys));

		// Create function definition in debug information

		llvm::DIScope *FContext = gContext.scopingStack.top()->getFile();	// temporary - should come from the location information
		unsigned LineNo = declLoc.first_line;
		unsigned ScopeLine = LineNo;
		auto spFlags = llvm::DISubprogram::toSPFlags(false, true, false);
		llvm::DISubprogram *SP = gContext.dbgBuilder->createFunction(
			FContext, SanitiseNameForDebug(func->getName()), llvm::StringRef(), gContext.scopingStack.top()->getFile(), LineNo,
			dbgFuncTy,
			ScopeLine,
			llvm::DINode::FlagZero, spFlags);
		func->setSubprogram(SP);

		gContext.scopingStack.push(SP);
	}
}

void CodeGenContext::EndFunctionDebugInfo()
{
	if (gContext.opts.generateDebug)
	{
		gContext.scopingStack.pop();
	}
}

void CodeGenContext::GenerateDisassmTables()
{
	std::map<std::string, std::map<llvm::APInt,std::string,myAPIntCompare> >::iterator tableIter;

	for (tableIter=disassemblyTable.begin();tableIter!=disassemblyTable.end();++tableIter)
	{
		llvm::APInt tableSize=tableIter->second.rbegin()->first;
		llvm::APInt tableSize32=tableSize.zextOrTrunc(32);
		// Create a global variable to indicate the max size of the table

		llvm::ConstantInt* const_int32_1 = getConstantInt(tableSize32+1);
		llvm::GlobalVariable* gvar_int32_DIS_max = makeGlobal(getIntType(32),true,llvm::GlobalValue::ExternalLinkage,const_int32_1,getSymbolPrefix()+"DIS_max_"+tableIter->first);

		// Create a global array to hold the table
		llvm::PointerType* PointerTy_5 = llvm::PointerType::get(getIntType(8), 0);
       	llvm::ArrayType* ArrayTy_4 = llvm::ArrayType::get(PointerTy_5, tableSize.getLimitedValue()+1);
		llvm::ConstantPointerNull* const_ptr_13 = llvm::ConstantPointerNull::get(PointerTy_5);	
		std::vector<llvm::Constant*> const_array_9_elems;

		std::map<llvm::APInt,std::string,myAPIntCompare>::iterator slot=tableIter->second.begin();
		llvm::APInt trackingSlot(tableSize.getBitWidth(),"0",16);

		while (slot!=tableIter->second.end())
		{
			if (CompareEquals(slot->first,trackingSlot))
			{
				llvm::ArrayType* ArrayTy_0 = llvm::ArrayType::get(getIntType(8), slot->second.length()-1);
				llvm::Constant* const_array_9 = getString(slot->second);
				llvm::GlobalVariable* gvar_array__str = makeGlobal(ArrayTy_0,true,llvm::GlobalValue::PrivateLinkage,const_array_9,getSymbolPrefix()+".str"+trackingSlot.toString(16,false));
				gvar_array__str->setAlignment(llvm::MaybeAlign(1));
  
				std::vector<llvm::Constant*> const_ptr_12_indices;
				llvm::ConstantInt* const_int64_13 = getConstantZero(64);
				const_ptr_12_indices.push_back(const_int64_13);
				const_ptr_12_indices.push_back(const_int64_13);
				llvm::Constant* const_ptr_12 = llvm::ConstantExpr::getGetElementPtr(nullptr,gvar_array__str, const_ptr_12_indices);

				const_array_9_elems.push_back(const_ptr_12);

				++slot;
			}
			else
			{
				const_array_9_elems.push_back(const_ptr_13);
			}
			++trackingSlot;
		}

		llvm::Constant* const_array_9 = llvm::ConstantArray::get(ArrayTy_4, const_array_9_elems);
		llvm::GlobalVariable* gvar_array_table = makeGlobal(ArrayTy_4,true,llvm::GlobalValue::ExternalLinkage,const_array_9, getSymbolPrefix()+"DIS_"+tableIter->first);
	}
}

/* Compile the AST into a module */
void CodeGenContext::generateCode(CBlock& root)
{
	root.prePass(*this);	/* Run a pre-pass on the code */
	root.codeGen(*this);	/* Generate complete code - starting at no block (global space) */

	if (isErrorFlagged())
	{
		return;
	}

	// Finalise disassembly arrays
	if (gContext.opts.generateDisassembly)
	{
		GenerateDisassmTables();
	}
	
}

/* -- Code Generation -- */

bool CodeGenContext::LookupBitVariable(BitVariable& outVar,const std::string& module, const std::string& name,const YYLTYPE &modLoc,const YYLTYPE &nameLoc)
{
	if ((currentBlock()!=nullptr) && (locals().find(name) != locals().end()))
	{
		outVar=locals()[name];
		outVar.refLoc = nameLoc;
	}
	else
	{
		if (globals().find(name) == globals().end())
		{
			if (m_includes.find(module)!=m_includes.end())
			{
				if (m_includes[module]->LookupBitVariable(outVar,"",name,modLoc,nameLoc))
				{
					if (outVar.pinType!=0)		// TODO: Globals Vars!
					{
						outVar.fromExternal=true;
						outVar.refLoc = CombineTokenLocations(nameLoc,modLoc);
						return true;
					}
					else
					{
						return gContext.ReportError(false, EC_InternalError, CombineTokenLocations(nameLoc, modLoc), "TODO: Globals");
					}
				}
				else
				{
					return gContext.ReportError(false, EC_ErrorAtLocation, CombineTokenLocations(nameLoc,modLoc), "undeclared variable %s%s", module.c_str(), name.c_str());
				}
			}
			else if (isRoot)
			{
				if (module == "")
				{
					return gContext.ReportError(false, EC_ErrorAtLocation, nameLoc, "undeclared variable %s", name.c_str());
				}
				else
				{
					return gContext.ReportError(false, EC_ErrorAtLocation, CombineTokenLocations(nameLoc,modLoc), "undeclared variable %s%s", module.c_str(), name.c_str());
				}
			}
			return false;
		}
		else
		{
			outVar=globals()[name];
			outVar.refLoc = nameLoc;
		}
	}

	outVar.fromExternal=false;
	return true;
}

llvm::Function* CodeGenContext::LookupFunctionInExternalModule(const std::string& moduleName, const std::string& name)
{
	return gContext.llvmModule->getFunction(moduleName+name);
}
