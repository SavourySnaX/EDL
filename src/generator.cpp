#include "yyltype.h"
#include "ast.h"
#include "generator.h"
#include "parser.hpp"
#include "optPasses.h"

#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/TargetRegistry.h"

const size_t PATH_DEFAULT_LEN=2048;

CIdentifier CAliasDeclaration::empty("");

extern YYLTYPE CombineTokenLocations(const YYLTYPE &a, const YYLTYPE &b);
extern void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...);
extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);
extern void PrintErrorDuplication(const YYLTYPE &location, const YYLTYPE &originalLocation, const char *errorstring, ...);
extern void PrintError(const char *errorstring, ...);

std::string SanitiseNameForDebug(llvm::StringRef inputName)
{
	std::string t = inputName;
	std::replace(t.begin(), t.end(), '.', '_');
	return t;
}

llvm::DIFile* CreateNewDbgFile(const char* filepath,llvm::DIBuilder* dbgBuilder)
{
	llvm::SmallString<PATH_DEFAULT_LEN> fullpath(filepath);
	llvm::sys::path::native(fullpath);
	llvm::sys::fs::make_absolute(fullpath);
	llvm::StringRef filename = llvm::sys::path::filename(fullpath);
	llvm::sys::path::remove_filename(fullpath);
	llvm::SmallString<32> Checksum;
	Checksum.clear();

	llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> CheckFileOrErr = llvm::MemoryBuffer::getFile(filepath);
	if (std::error_code EC = CheckFileOrErr.getError())
	{
		assert(0 && "failed to read file");
	}
	llvm::MemoryBuffer &MemBuffer = *CheckFileOrErr.get();

	llvm::MD5 Hash;
	llvm::MD5::MD5Result Result;

	Hash.update(MemBuffer.getBuffer());
	Hash.final(Result);

	Hash.stringifyResult(Result, Checksum);

	return dbgBuilder->createFile(filename, fullpath, llvm::DIFile::CSK_MD5, Checksum);
}

llvm::Value* UndefinedStateError(StateIdentList &stateIdents,CodeGenContext &context)
{
	YYLTYPE combined = stateIdents[0]->nameLoc;
	for (int a = 0; a < stateIdents.size(); a++)
	{
		combined = CombineTokenLocations(combined, stateIdents[a]->nameLoc);
	}
	PrintErrorFromLocation(combined, "Unknown state requested");
	context.FlagError();
	return nullptr;
}

CodeGenContext::CodeGenContext(GlobalContext& globalContext,CodeGenContext* parent) : gContext(globalContext)
{
	moduleName="";
	if (!parent) 
	{ 
		std::string err;
		std::unique_ptr<llvm::Module> Owner = llvm::make_unique<llvm::Module>("root", gContext.getLLVMContext());
		globalContext.llvmModule = Owner.get();
		dbgBuilder = new llvm::DIBuilder(*globalContext.llvmModule);
		globalContext.llvmExecutionEngine = llvm::EngineBuilder(std::move(Owner)).setErrorStr(&err).setMCJITMemoryManager(llvm::make_unique<llvm::SectionMemoryManager>()).create();
		if (globalContext.llvmExecutionEngine == nullptr)
		{
			std::cerr << err << std::endl;
			exit(1);
		}
		isRoot=true; 
	}
	else
	{
		dbgBuilder = parent->dbgBuilder;
		compileUnit = parent->compileUnit; // not sure about this yet
		isRoot=false;
	}
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

void CodeGenContext::StartFunctionDebugInfo(llvm::Function* func, YYLTYPE& declLoc)
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
			EltTys.push_back(dbgBuilder->createBasicType(name, retType->getIntegerBitWidth(), 0));
		}
		else
		{
			PrintErrorFromLocation(declLoc, "Internal Compiler error, return type is not void or integer");
			return;
		}
		for (const auto& arg : func->getFunctionType()->params())
		{
			if (!arg->isIntegerTy())
			{
				PrintErrorFromLocation(declLoc, "Internal Compiler error, arg type is not integer");
				return;
			}
			std::string name = "Bit" + std::to_string(arg->getIntegerBitWidth());
			EltTys.push_back(dbgBuilder->createBasicType(name, arg->getIntegerBitWidth(), 0));
		}

		llvm::DISubroutineType* dbgFuncTy = dbgBuilder->createSubroutineType(dbgBuilder->getOrCreateTypeArray(EltTys));

		// Create function definition in debug information

		llvm::DIScope *FContext = gContext.scopingStack.top()->getFile();	// temporary - should come from the location information
		unsigned LineNo = declLoc.first_line;
		unsigned ScopeLine = LineNo;
		llvm::DISubprogram *SP = dbgBuilder->createFunction(
			FContext, SanitiseNameForDebug(func->getName()), llvm::StringRef(), gContext.scopingStack.top()->getFile(), LineNo,
			dbgFuncTy,
			false /* internal linkage */, true /* definition */, ScopeLine,
			llvm::DINode::FlagZero, false);
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

		llvm::GlobalVariable* gvar_int32_DIS_max = makeGlobal(getIntType(32),true,llvm::GlobalValue::ExternalLinkage,nullptr,getSymbolPrefix()+"DIS_max_"+tableIter->first);
		llvm::ConstantInt* const_int32_1 = getConstantInt(tableSize32+1);
		gvar_int32_DIS_max->setInitializer(const_int32_1);

		// Create a global array to hold the table
		llvm::PointerType* PointerTy_5 = llvm::PointerType::get(getIntType(8), 0);
       	llvm::ArrayType* ArrayTy_4 = llvm::ArrayType::get(PointerTy_5, tableSize.getLimitedValue()+1);
		llvm::ConstantPointerNull* const_ptr_13 = llvm::ConstantPointerNull::get(PointerTy_5);	
		llvm::GlobalVariable* gvar_array_table = makeGlobal(ArrayTy_4,true,llvm::GlobalValue::ExternalLinkage,nullptr, getSymbolPrefix()+"DIS_"+tableIter->first);
		std::vector<llvm::Constant*> const_array_9_elems;

		std::map<llvm::APInt,std::string,myAPIntCompare>::iterator slot=tableIter->second.begin();
		llvm::APInt trackingSlot(tableSize.getBitWidth(),"0",16);

		while (slot!=tableIter->second.end())
		{
			if (CompareEquals(slot->first,trackingSlot))
			{
				llvm::ArrayType* ArrayTy_0 = llvm::ArrayType::get(getIntType(8), slot->second.length()-1);
				llvm::GlobalVariable* gvar_array__str = makeGlobal(ArrayTy_0,true,llvm::GlobalValue::PrivateLinkage,0,getSymbolPrefix()+".str"+trackingSlot.toString(16,false));
				gvar_array__str->setAlignment(1);
  
				llvm::Constant* const_array_9 = getString(slot->second);
				std::vector<llvm::Constant*> const_ptr_12_indices;
				llvm::ConstantInt* const_int64_13 = getConstantZero(64);
				const_ptr_12_indices.push_back(const_int64_13);
				const_ptr_12_indices.push_back(const_int64_13);
				llvm::Constant* const_ptr_12 = llvm::ConstantExpr::getGetElementPtr(nullptr,gvar_array__str, const_ptr_12_indices);

				gvar_array__str->setInitializer(const_array_9);

				const_array_9_elems.push_back(const_ptr_12);

				++slot;
			}
			else
			{
				const_array_9_elems.push_back(const_ptr_13);
			}
			trackingSlot++;
		}

		llvm::Constant* const_array_9 = llvm::ConstantArray::get(ArrayTy_4, const_array_9_elems);
		gvar_array_table->setInitializer(const_array_9);
	}
}

/* Compile the AST into a module */
void CodeGenContext::generateCode(CBlock& root)
{
	if (isRoot)
	{
		if (gContext.opts.symbolModifier)
		{
			gContext.symbolPrefix=gContext.opts.symbolModifier;
		}

		// Testing - setup a compile unit for our root module
		if (gContext.opts.generateDebug)
		{
			gContext.scopingStack.push(CreateNewDbgFile(gContext.opts.inputFile, dbgBuilder));

			compileUnit = dbgBuilder->createCompileUnit(/*0x9999*/llvm::dwarf::DW_LANG_C99, gContext.scopingStack.top()->getFile(), "edlVxx", gContext.opts.optimisationLevel, ""/*command line flags*/, 0);

#if defined(EDL_PLATFORM_MSVC)
			gContext.llvmModule->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
#else
			gContext.llvmModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);
#endif
			gContext.llvmModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
		}

		llvm::PointerType* PointerTy_4 = llvm::PointerType::get(getIntType(8), 0);

		std::vector<llvm::Type*>FuncTy_8_args;
		FuncTy_8_args.push_back(PointerTy_4);
		llvm::FunctionType* FuncTy_8 = llvm::FunctionType::get(/*Result=*/getIntType(32),/*Params=*/FuncTy_8_args,/*isVarArg=*/false);

		std::vector<llvm::Type*>FuncTy_6_args;
		FuncTy_6_args.push_back(getIntType(32));
		llvm::FunctionType* FuncTy_6 = llvm::FunctionType::get(/*Result=*/getIntType(32),/*Params=*/FuncTy_6_args,/*isVarArg=*/false);

		gContext.debugTraceChar = makeFunction(FuncTy_6,llvm::GlobalValue::ExternalLinkage,"putchar"); // (external, no body)
		
		gContext.debugTraceMissing = makeFunction(FuncTy_6,llvm::GlobalValue::ExternalLinkage,getSymbolPrefix()+"missing"); // (external, no body)
		
		std::vector<llvm::Type*>FuncTy_9_args;
		FuncTy_9_args.push_back(getIntType(8));								//bitWidth (max 32)
		FuncTy_9_args.push_back(getIntType(32));							//value
		FuncTy_9_args.push_back(llvm::PointerType::get(getIntType(8), 0));	// char* busname
		FuncTy_9_args.push_back(getIntType(32));							//dataDecode Width
		FuncTy_9_args.push_back(getIntType(8));								// 0 / 1 - if 1 indicates the last tap in a connection list
		llvm::FunctionType* FuncTy_9 = llvm::FunctionType::get(/*Result=*/getVoidType(),/*Params=*/FuncTy_9_args, false);

		gContext.debugBusTap = makeFunction(FuncTy_9,llvm::GlobalValue::ExternalLinkage,getSymbolPrefix()+"BusTap"); // (external, no body)
	}

	root.prePass(*this);	/* Run a pre-pass on the code */
	root.codeGen(*this);	/* Generate complete code - starting at no block (global space) */

	// Finalise disassembly arrays
	if (gContext.opts.generateDisassembly)
	{
		GenerateDisassmTables();
	}
	
	if (isErrorFlagged())
	{
		return;
	}

	if (isRoot)
	{
		if (gContext.opts.generateDebug)
		{
			gContext.scopingStack.pop();
			assert(gContext.scopingStack.empty() && "Programming error");

			dbgBuilder->finalize();

			llvm::NamedMDNode *IdentMetadata = gContext.llvmModule->getOrInsertNamedMetadata("llvm.ident");
			std::string Version = "EDLvxx";
			llvm::LLVMContext &Ctx = gContext.llvmModule->getContext();

			llvm::Metadata *IdentNode[] = { llvm::MDString::get(Ctx, Version) };
			IdentMetadata->addOperand(llvm::MDNode::get(Ctx, IdentNode));
		}
		gContext.llvmModule->setDataLayout(gContext.llvmExecutionEngine->getDataLayout());
		gContext.llvmModule->setTargetTriple(llvm::sys::getDefaultTargetTriple()/*"x86_64--windows-msvc"*/);	// required for codeview debug information to be output AT ALL (need to update object file output)

		/* Print the bytecode in a human-readable format 
		   to see if our program compiled properly
		   */
		llvm::legacy::PassManager pm;

		pm.add(llvm::createVerifierPass());
		
		if (gContext.opts.optimisationLevel>0)
		{
			// Add an appropriate TargetData instance for this module...

			for (int a=0;a<2;a++)		// We add things twice, as the statereferencesquasher will make more improvements once inlining has happened
			{
				if (gContext.opts.optimisationLevel>1)
				{
					pm.add(new StateReferenceSquasher(this));		// Custom pass designed to remove redundant loads of the current state (since it can only be modified in one place)
					pm.add(new InOutReferenceSquasher(this));		// Custom pass designed to remove redundant loads of the current state (since it can only be modified in one place)
				}

				//pm.add(createLowerSetJmpPass());          // Lower llvm.setjmp/.longjmp

				// Propagate constants at call sites into the functions they call.  This
				// opens opportunities for globalopt (and inlining) by substituting function
				// pointers passed as arguments to direct uses of functions.  
				pm.add(llvm::createIPSCCPPass());

				// Now that we internalized some globals, see if we can hack on them!
				pm.add(llvm::createGlobalOptimizerPass());

				// Linking modules together can lead to duplicated global constants, only
				// keep one copy of each constant...
				pm.add(llvm::createConstantMergePass());

				// Remove unused arguments from functions...
				pm.add(llvm::createDeadArgEliminationPass());

				// Reduce the code after globalopt and ipsccp.  Both can open up significant
				// simplification opportunities, and both can propagate functions through
				// function pointers.  When this happens, we often have to resolve varargs
				// calls, etc, so let instcombine do this.
				pm.add(llvm::createInstructionCombiningPass());
				//pm.add(createFunctionInliningPass()); // Inline small functions
				pm.add(llvm::createPruneEHPass());              // Remove dead EH info
				pm.add(llvm::createGlobalDCEPass());            // Remove dead functions

				// If we didn't decide to inline a function, check to see if we can
				// transform it to pass arguments by value instead of by reference.
				pm.add(llvm::createArgumentPromotionPass());

				// The IPO passes may leave cruft around.  Clean up after them.
				pm.add(llvm::createInstructionCombiningPass());
				pm.add(llvm::createJumpThreadingPass());        // Thread jumps.
//				pm.add(createScalarReplAggregatesPass()); // Break up allocas

				// Run a few AA driven optimizations here and now, to cleanup the code.
				//pm.add(createFunctionAttrsPass());        // Add nocapture
				//pm.add(createGlobalsModRefPass());        // IP alias analysis
				pm.add(llvm::createLICMPass());                 // Hoist loop invariants
				pm.add(llvm::createGVNPass());                  // Remove common subexprs
				pm.add(llvm::createMemCpyOptPass());            // Remove dead memcpy's
				pm.add(llvm::createDeadStoreEliminationPass()); // Nuke dead stores

				// Cleanup and simplify the code after the scalar optimizations.
				pm.add(llvm::createInstructionCombiningPass());
				pm.add(llvm::createJumpThreadingPass());        // Thread jumps.
				pm.add(llvm::createPromoteMemoryToRegisterPass()); // Cleanup after threading.


				// Delete basic blocks, which optimization passes may have killed...
				pm.add(llvm::createCFGSimplificationPass());

				// Now that we have optimized the program, discard unreachable functions...
				pm.add(llvm::createGlobalDCEPass());
				pm.add(llvm::createCFGSimplificationPass());    // Clean up disgusting code
				pm.add(llvm::createPromoteMemoryToRegisterPass());// Kill useless allocas
				pm.add(llvm::createGlobalOptimizerPass());      // Optimize out global vars
				pm.add(llvm::createGlobalDCEPass());            // Remove unused fns and globs
				pm.add(llvm::createIPConstantPropagationPass());// IP Constant Propagation
				pm.add(llvm::createDeadArgEliminationPass());   // Dead argument elimination
				pm.add(llvm::createInstructionCombiningPass()); // Clean up after IPCP & DAE
				pm.add(llvm::createCFGSimplificationPass());    // Clean up after IPCP & DAE

				pm.add(llvm::createPruneEHPass());              // Remove dead EH info
				//pm.add(createFunctionAttrsPass());        // Deduce function attrs

				//  if (!DisableInline)
				pm.add(llvm::createFunctionInliningPass());   // Inline small functions
				pm.add(llvm::createArgumentPromotionPass());    // Scalarize uninlined fn args

				pm.add(llvm::createInstructionCombiningPass()); // Cleanup for scalarrepl.
				pm.add(llvm::createJumpThreadingPass());        // Thread jumps.
				pm.add(llvm::createCFGSimplificationPass());    // Merge & remove BBs
//				pm.add(createScalarReplAggregatesPass()); // Break up aggregate allocas
				pm.add(llvm::createInstructionCombiningPass()); // Combine silly seq's

				pm.add(llvm::createTailCallEliminationPass());  // Eliminate tail calls
				pm.add(llvm::createCFGSimplificationPass());    // Merge & remove BBs
				pm.add(llvm::createReassociatePass());          // Reassociate expressions
				pm.add(llvm::createLoopRotatePass());
				pm.add(llvm::createLICMPass());                 // Hoist loop invariants
				pm.add(llvm::createLoopUnswitchPass());         // Unswitch loops.
				// FIXME : Removing instcombine causes nestedloop regression.
				pm.add(llvm::createInstructionCombiningPass());
				pm.add(llvm::createIndVarSimplifyPass());       // Canonicalize indvars
				pm.add(llvm::createLoopDeletionPass());         // Delete dead loops
				pm.add(llvm::createLoopUnrollPass());           // Unroll small loops
				pm.add(llvm::createInstructionCombiningPass()); // Clean up after the unroller
				pm.add(llvm::createGVNPass());                  // Remove redundancies
				pm.add(llvm::createMemCpyOptPass());            // Remove memcpy / form memset
				pm.add(llvm::createSCCPPass());                 // Constant prop with SCCP

				// Run instcombine after redundancy elimination to exploit opportunities
				// opened up by them.
				pm.add(llvm::createInstructionCombiningPass());

				pm.add(llvm::createDeadStoreEliminationPass()); // Delete dead stores
				pm.add(llvm::createAggressiveDCEPass());        // Delete dead instructions
				pm.add(llvm::createCFGSimplificationPass());    // Merge & remove BBs
				pm.add(llvm::createStripDeadPrototypesPass());  // Get rid of dead prototypes
	//			pm.add(createDeadTypeEliminationPass());  // Eliminate dead types
				pm.add(llvm::createConstantMergePass());        // Merge dup global constants

				// Make sure everything is still good.
				pm.add(llvm::createVerifierPass());
			}
		}
		if (gContext.opts.outputFile != nullptr)
		{
			std::string error;
			auto triple = llvm::sys::getDefaultTargetTriple();
			auto target = llvm::TargetRegistry::lookupTarget(triple,error);
			if (!target)
			{
				FlagError();
				std::cerr << error << std::endl;
				return;
			}
			llvm::TargetOptions opt;
			auto rm = llvm::Optional<llvm::Reloc::Model>();
			auto targetMachine = target->createTargetMachine(triple, "generic", "", opt, rm);
			std::error_code ec;
			llvm::raw_fd_ostream dest(gContext.opts.outputFile, ec,llvm::sys::fs::F_None);
			if (ec)
			{
				FlagError();
				std::cerr << ec.message() << std::endl;
				return;
			}
			if (targetMachine->addPassesToEmitFile(pm, dest, llvm::TargetMachine::CGFT_ObjectFile))
			{
				FlagError();
				std::cerr << "Cannot emit object file" << std::endl;
				return;
			}
			pm.run(*gContext.llvmModule);
			dest.flush();
		}
		else
		{
			pm.add(llvm::createPrintModulePass(llvm::outs()));
			pm.run(*gContext.llvmModule);
		}
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
						PrintErrorFromLocation(CombineTokenLocations(nameLoc, modLoc), "TODO: Globals");
						FlagError();
						return false;
					}
				}
				else
				{
					PrintErrorFromLocation(CombineTokenLocations(nameLoc,modLoc), "undeclared variable %s%s", module.c_str(), name.c_str());
					FlagError();
					return false;
				}
			}
			else if (isRoot)
			{
				if (module == "")
				{
					PrintErrorFromLocation(nameLoc, "undeclared variable %s", name.c_str());
				}
				else
				{
					PrintErrorFromLocation(CombineTokenLocations(nameLoc,modLoc), "undeclared variable %s%s", module.c_str(), name.c_str());
				}
				FlagError();
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


//////////////////////////////////////////////////////////////////////////

llvm::Function*	CodeGenContext::makeFunction(llvm::FunctionType* fType, llvm::Function::LinkageTypes fLinkType, const llvm::Twine& fName)
{
	llvm::Function* func = llvm::Function::Create(fType, fLinkType, fName, gContext.llvmModule);
	func->setCallingConv(llvm::CallingConv::C);
	func->setDoesNotThrow();
	return func;
}

llvm::GlobalVariable* CodeGenContext::makeGlobal(llvm::Type* gType, bool isConstant, llvm::GlobalVariable::LinkageTypes gLinkType, llvm::Constant* gInitialiser, const llvm::Twine& gName)
{
	return new llvm::GlobalVariable(*gContext.llvmModule, gType, isConstant, gLinkType, gInitialiser, gName);
}
