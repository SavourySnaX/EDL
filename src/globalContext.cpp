#include <iostream>

#include "yyltype.h"
#include "optPasses.h"
#include "globalContext.h"
#include "generator.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/LinkAllIR.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/Pass.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

const size_t PATH_DEFAULT_LEN=2048;

bool GlobalContext::InitGlobalCodeGen()
{
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	if (opts.symbolModifier)
	{
		symbolPrefix = opts.symbolModifier;
	}

	irBuilder = new llvm::IRBuilder<>(llvmContext);
	dbgBuilder = new llvm::DIBuilder(*llvmModule);

	// Setup Debug Contexts
	if (opts.generateDebug)
	{
		scopingStack.push(CreateNewDbgFile(opts.inputFile));

		compileUnit = dbgBuilder->createCompileUnit(/*0x9999*/llvm::dwarf::DW_LANG_C99, scopingStack.top()->getFile(), "edlVxx", opts.optimisationLevel, ""/*command line flags*/, 0);

#if defined(EDL_PLATFORM_MSVC)
		llvmModule->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
#else
		llvmModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);
#endif
		llvmModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
	}

	// Setup external (C-Side) function declerations
	llvm::PointerType* PointerTy_4 = llvm::PointerType::get(getIntType(8), 0);

	std::vector<llvm::Type*>FuncTy_8_args;
	FuncTy_8_args.push_back(PointerTy_4);
	llvm::FunctionType* FuncTy_8 = llvm::FunctionType::get(/*Result=*/getIntType(32),/*Params=*/FuncTy_8_args,/*isVarArg=*/false);

	std::vector<llvm::Type*>FuncTy_6_args;
	FuncTy_6_args.push_back(getIntType(32));
	llvm::FunctionType* FuncTy_6 = llvm::FunctionType::get(/*Result=*/getIntType(32),/*Params=*/FuncTy_6_args,/*isVarArg=*/false);

	debugTraceChar = makeFunction(FuncTy_6, llvm::GlobalValue::ExternalLinkage, "putchar"); // (external, no body)

	debugTraceMissing = makeFunction(FuncTy_6, llvm::GlobalValue::ExternalLinkage, symbolPrefix + "missing"); // (external, no body)

	std::vector<llvm::Type*>FuncTy_9_args;
	FuncTy_9_args.push_back(getIntType(8));								//bitWidth (max 32)
	FuncTy_9_args.push_back(getIntType(32));							//value
	FuncTy_9_args.push_back(llvm::PointerType::get(getIntType(8), 0));	// char* busname
	FuncTy_9_args.push_back(getIntType(32));							//dataDecode Width
	FuncTy_9_args.push_back(getIntType(8));								// 0 / 1 - if 1 indicates the last tap in a connection list
	llvm::FunctionType* FuncTy_9 = llvm::FunctionType::get(/*Result=*/getVoidType(),/*Params=*/FuncTy_9_args, false);

	debugBusTap = makeFunction(FuncTy_9, llvm::GlobalValue::ExternalLinkage, symbolPrefix + "BusTap"); // (external, no body)

	return true;
}

bool GlobalContext::SetupPassesAndRun(CodeGenContext& rootContext)
{
	// Setup target machine
	std::string error;
	auto triple = llvm::sys::getDefaultTargetTriple();
	auto target = llvm::TargetRegistry::lookupTarget(triple, error);
	if (!target)
	{
		std::cerr << error << std::endl;
		return false;
	}
	llvm::TargetOptions opt;
	auto rm = llvm::Optional<llvm::Reloc::Model>();
	auto targetMachine = target->createTargetMachine(triple, "generic", "", opt, rm);

	llvmModule->setTargetTriple(triple);	// required for codeview debug information to be output AT ALL (need to update object file output)
	llvmModule->setDataLayout(targetMachine->createDataLayout());
	// Setup Passes (either write to file or dump to stdout)
	llvm::legacy::PassManager pm;

	pm.add(llvm::createVerifierPass());

	if (opts.optimisationLevel > 0)
	{
		for (int a = 0; a < 2; a++)		// We add things twice, as the statereferencesquasher will make more improvements once inlining has happened
		{
			if (opts.optimisationLevel > 1)
			{
				pm.add(new StateReferenceSquasher(&rootContext));		// Custom pass designed to remove redundant loads of the current state (since it can only be modified in one place)
				pm.add(new InOutReferenceSquasher(&rootContext));		// Custom pass designed to remove redundant loads of the current state (since it can only be modified in one place)
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
	if (opts.outputFile != nullptr)
	{
		std::error_code ec;
		llvm::raw_fd_ostream dest(opts.outputFile, ec, llvm::sys::fs::F_None);
		if (ec)
		{
			std::cerr << ec.message() << std::endl;
			return false;
		}
		if (targetMachine->addPassesToEmitFile(pm, dest, nullptr, llvm::TargetMachine::CGFT_ObjectFile))
		{
			std::cerr << "Cannot emit object file" << std::endl;
			return false;
		}
		pm.run(*llvmModule);
		dest.flush();
	}
	else
	{
		pm.add(llvm::createPrintModulePass(llvm::outs()));
		pm.run(*llvmModule);
	}
	return true;
}

bool GlobalContext::FinaliseCodeGen(CodeGenContext& rootContext)
{
	//check for error flagged first
	if (errorFlagged)
		return false;

	if (opts.generateDebug)
	{
		scopingStack.pop();
		assert(scopingStack.empty() && "Programming error");

		dbgBuilder->finalize();

		llvm::NamedMDNode *IdentMetadata = llvmModule->getOrInsertNamedMetadata("llvm.ident");
		std::string Version = "EDLvxx";
		llvm::LLVMContext &Ctx = llvmModule->getContext();

		llvm::Metadata *IdentNode[] = { llvm::MDString::get(Ctx, Version) };
		IdentMetadata->addOperand(llvm::MDNode::get(Ctx, IdentNode));
	}

	return SetupPassesAndRun(rootContext);
}

bool GlobalContext::generateCode(CBlock& root)
{
	std::unique_ptr<llvm::Module> Owner = llvm::make_unique<llvm::Module>("root", llvmContext);
	llvmModule = Owner.get();

	if (!InitGlobalCodeGen())
		return false;

	CodeGenContext rootContext(*this,nullptr);
	rootContext.generateCode(root);

	return FinaliseCodeGen(rootContext);
}

llvm::Value* GlobalContext::ReportUndefinedStateError(StateIdentList &stateIdents)
{
	YYLTYPE combined = stateIdents[0]->nameLoc;
	for (int a = 0; a < stateIdents.size(); a++)
	{
		combined = CombineTokenLocations(combined, stateIdents[a]->nameLoc);
	}
	return ReportError(nullptr, EC_ErrorAtLocation, combined, "Unknown state requested");
}

llvm::DIFile* GlobalContext::CreateNewDbgFile(const char* filepath)
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
		return ReportError(nullptr, EC_InternalError, YYLTYPE(), "failed to read file : %s",filepath);
	}
	llvm::MemoryBuffer &MemBuffer = *CheckFileOrErr.get();

	llvm::MD5 Hash;
	llvm::MD5::MD5Result Result;

	Hash.update(MemBuffer.getBuffer());
	Hash.final(Result);

	Hash.stringifyResult(Result, Checksum);

	return dbgBuilder->createFile(filename, fullpath);
}

//////////////////////////////////////////////////////////////////////////

llvm::Function*	GlobalContext::makeFunction(llvm::FunctionType* fType, llvm::Function::LinkageTypes fLinkType, const llvm::Twine& fName)
{
	llvm::Function* func = llvm::Function::Create(fType, fLinkType, fName, llvmModule);
	func->setCallingConv(llvm::CallingConv::C);
	func->setDoesNotThrow();
	return func;
}

llvm::GlobalVariable* GlobalContext::makeGlobal(llvm::Type* gType, bool isConstant, llvm::GlobalVariable::LinkageTypes gLinkType, llvm::Constant* gInitialiser, const llvm::Twine& gName)
{
	return new llvm::GlobalVariable(*llvmModule, gType, isConstant, gLinkType, gInitialiser, gName);
}
