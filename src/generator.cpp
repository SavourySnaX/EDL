#include "yyltype.h"
#include "ast.h"
#include "generator.h"
#include "parser.hpp"

#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

const size_t PATH_DEFAULT_LEN=2048;

llvm::LLVMContext TheContext;
static std::map<Function*,Function*> g_connectFunctions;		// Global for now

extern YYLTYPE CombineTokenLocations(const YYLTYPE &a, const YYLTYPE &b);
extern void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...);
extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);
extern void PrintErrorDuplication(const YYLTYPE &location, const YYLTYPE &originalLocation, const char *errorstring, ...);
extern void PrintError(const char *errorstring, ...);

std::string SanitiseNameForDebug(StringRef inputName)
{
	std::string t = inputName;
	std::replace(t.begin(), t.end(), '.', '_');
	return t;
}

DIFile* CreateNewDbgFile(const char* filepath,DIBuilder* dbgBuilder)
{
	SmallString<PATH_DEFAULT_LEN> fullpath(filepath);
	sys::path::native(fullpath);
	sys::fs::make_absolute(fullpath);
	StringRef filename = sys::path::filename(fullpath);
	sys::path::remove_filename(fullpath);
	SmallString<32> Checksum;
	Checksum.clear();

	ErrorOr<std::unique_ptr<MemoryBuffer>> CheckFileOrErr = MemoryBuffer::getFile(filepath);
	if (std::error_code EC = CheckFileOrErr.getError())
	{
		assert(0 && "failed to read file");
	}
	MemoryBuffer &MemBuffer = *CheckFileOrErr.get();

	llvm::MD5 Hash;
	llvm::MD5::MD5Result Result;

	Hash.update(MemBuffer.getBuffer());
	Hash.final(Result);

	Hash.stringifyResult(Result, Checksum);

	return dbgBuilder->createFile(filename, fullpath, DIFile::CSK_MD5, Checksum);
}

Value* UndefinedStateError(StateIdentList &stateIdents,CodeGenContext &context)
{
	YYLTYPE combined = stateIdents[0]->nameLoc;
	for (int a = 0; a < stateIdents.size(); a++)
	{
		combined = CombineTokenLocations(combined, stateIdents[a]->nameLoc);
	}
	PrintErrorFromLocation(combined, "Unknown state requested");
	context.errorFlagged = true;
	return nullptr;
}

int optlevel=0;


#define DEBUG_STATE_SQUASH	0

namespace 
{
	struct StateReferenceSquasher : public FunctionPass 
	{
		CodeGenContext *ourContext;

		static char ID;
		StateReferenceSquasher() : ourContext(nullptr),FunctionPass(ID) {}
		StateReferenceSquasher(CodeGenContext* context) : ourContext(context),FunctionPass(ID) {}

		bool DoWork(Function &F,CodeGenContext* context)
		{
			std::map<std::string, StateVariable>::iterator stateVars;
			bool doneSomeWork=false;
			for (stateVars=context->states().begin();stateVars!=context->states().end();++stateVars)
			{
#if DEBUG_STATE_SQUASH
				std::cerr << "-" << F.getName().str() << " " << stateVars->first << std::endl;
#endif
				// Search the instructions in the function, and find Load instructions
				Value* foundReference=nullptr;
				for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
				{
					LoadInst* iRef = dyn_cast<LoadInst>(&*I);
					if (iRef)
					{
						// We have a load instruction, we need to see if its operand points at our global state variable
						if (iRef->getOperand(0)==stateVars->second.currentState)
						{
							if (foundReference)
							{
#if DEBUG_STATE_SQUASH
								std::cerr << "--" << F.getName().str() << " " << iRef->getName().str() << std::endl;
#endif
								// Replace all uses of this instruction with original result,
								// I don't remove the current instruction - its upto a later pass to remove it
								iRef->replaceAllUsesWith(foundReference);//BinaryOperator::Create (Instruction::Or,foundReference,foundReference,"",iRef));
								doneSomeWork=true;
							}
							else
							{
#if DEBUG_STATE_SQUASH
								std::cerr << "==" << F.getName().str() << " " << iRef->getName().str() << std::endl;
#endif
								foundReference=iRef;
							}
						}
					}
					StoreInst* sRef = dyn_cast<StoreInst>(&*I);
					if (sRef)
					{
						// We have a store instruction, we need to see if its operand points at our global state variable
						if (sRef->getOperand(1)==stateVars->second.currentState)
						{
							foundReference=sRef->getOperand(0);
						}
					}
				}

			}
			return doneSomeWork;
		}

		virtual bool runOnFunction(Function &F) 
		{
			bool doneSomeWork=false;

    			std::map<std::string, CodeGenContext*>::iterator includeIter;

#if DEBUG_STATE_SQUASH
			std::cerr << "Running stateReferenceSquasher" << std::endl;
#endif
			doneSomeWork|=DoWork(F,ourContext);
			for (includeIter=ourContext->m_includes.begin();includeIter!=ourContext->m_includes.end();++includeIter)
			{
#if DEBUG_STATE_SQUASH
				std::cerr << F.getName().str() << " " << includeIter->second->states().size() << std::endl;
#endif
				doneSomeWork|=DoWork(F,includeIter->second);
			}

			return doneSomeWork;
		}
	};

	char StateReferenceSquasher::ID = 0;
	static RegisterPass<StateReferenceSquasher> X("StateReferenceSquasher", "Squashes multiple accesses to the same state, which should allow if/else behaviour", false, false);

	// Below is similar to above - Essentially pin variables that are in cannot be modified during execution - this hoists all the loads into the entry block
	struct InOutReferenceSquasher : public FunctionPass 
	{
		CodeGenContext *ourContext;

		static char ID;
		InOutReferenceSquasher() : ourContext(nullptr),FunctionPass(ID) {}
		InOutReferenceSquasher(CodeGenContext* context) : ourContext(context),FunctionPass(ID) {}

		bool DoWork(Function &F,CodeGenContext* context)
		{
			BasicBlock& hoistHere = F.getEntryBlock();
			TerminatorInst* hoistBefore = hoistHere.getTerminator();

			// InOut squasher re-orders the inputs, this is not a safe operation for CONNECT lists, so we exclude them here
			if (g_connectFunctions.find(&F)!=g_connectFunctions.end())
			{
				return false;
			}

			std::map<std::string, BitVariable>::iterator bitVars;
			bool doneSomeWork=false;
			for (bitVars=context->globals().begin();bitVars!=context->globals().end();++bitVars)
			{
				if (bitVars->second.pinType==TOK_IN/* || bitVars->second.pinType==TOK_OUT*/)
				{
#if DEBUG_STATE_SQUASH
					std::cerr << "-" << F.getName().str() << " " << bitVars->first << std::endl;
#endif
					// Search the instructions in the function, and find Load instructions
					Value* foundReference=nullptr;
				
					// Find and hoist all Pin loads to entry (avoids need to phi - hopefully llvm later passes will clean up)
					bool hoisted=false;

					do
					{
						hoisted=false;
						for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
						{
							LoadInst* iRef = dyn_cast<LoadInst>(&*I);
							if (iRef)
							{
								// We have a load instruction, we need to see if its operand points at our global state variable
								if (iRef->getOperand(0)==bitVars->second.value)
								{
									// First load for parameter - if its not in the entry basic block, move it
									if (F.getEntryBlock().getName()!=iRef->getParent()->getName())
									{
#if DEBUG_STATE_SQUASH
										std::cerr << "==" << F.getName().str() << " " << iRef->getName().str() << std::endl;
										std::cerr << "BB" << F.getEntryBlock().getName().str() << " " << iRef->getParent()->getName().str() << std::endl;
										std::cerr << "Needs Hoist" << std::endl;
#endif
										iRef->removeFromParent();
										iRef->insertBefore(hoistBefore);
										hoisted=true;
										break;
									}
								}
							}
						}
					}
					while (hoisted);

					// Do state squasher
					for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
					{
						LoadInst* iRef = dyn_cast<LoadInst>(&*I);
						if (iRef)
						{
							// We have a load instruction, we need to see if its operand points at our global state variable
							if (iRef->getOperand(0)==bitVars->second.value)
							{
								if (foundReference)
								{
#if DEBUG_STATE_SQUASH
									std::cerr << "--" << F.getName().str() << " " << iRef->getName().str() << std::endl;
#endif
									// Replace all uses of this instruction with original result,
									// I don't remove the current instruction - its upto a later pass to remove it
									iRef->replaceAllUsesWith(foundReference);//BinaryOperator::Create (Instruction::Or,foundReference,foundReference,"",iRef));
									doneSomeWork=true;
								}
								else
								{
#if DEBUG_STATE_SQUASH
									std::cerr << "==" << F.getName().str() << " " << iRef->getName().str() << std::endl;
#endif
									foundReference=iRef;
								}
							}
						}
						StoreInst* sRef = dyn_cast<StoreInst>(&*I);
						if (sRef)
						{
							// We have a store instruction, we need to see if its operand points at our global state variable
							if (sRef->getOperand(1)==bitVars->second.value)
							{
								foundReference=sRef->getOperand(0);
							}
						}
					}
				}

			}
			return doneSomeWork;
		}

		virtual bool runOnFunction(Function &F) 
		{
			bool doneSomeWork=false;

    			std::map<std::string, CodeGenContext*>::iterator includeIter;

#if DEBUG_STATE_SQUASH
			std::cerr << "Running globalReferenceSquasher" << std::endl;
#endif
			doneSomeWork|=DoWork(F,ourContext);
			for (includeIter=ourContext->m_includes.begin();includeIter!=ourContext->m_includes.end();++includeIter)
			{
#if DEBUG_STATE_SQUASH
				std::cerr << F.getName().str() << " " << includeIter->second->globals().size() << std::endl;
#endif
				doneSomeWork|=DoWork(F,includeIter->second);
			}

			return doneSomeWork;
		}
	};

	char InOutReferenceSquasher::ID = 1;
	static RegisterPass<InOutReferenceSquasher> XX("InOutReferenceSquasher", "Squashes multiple accesses to the same in/out pin, which should allow if/else behaviour", false, false);

}

using namespace std;

CodeGenContext::CodeGenContext(GlobalContext& globalContext,CodeGenContext* parent) : gContext(globalContext)
{
	moduleName="";
	if (!parent) 
	{ 
		std::string err;
		std::unique_ptr<Module> Owner = llvm::make_unique<Module>("root", TheContext);
		module = Owner.get();
		dbgBuilder = new DIBuilder(*module);
		ee = EngineBuilder(std::move(Owner)).setErrorStr(&err).setMCJITMemoryManager(llvm::make_unique<SectionMemoryManager>()).create();
		if (!ee)
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
		module=parent->module;
		ee=parent->ee;
		debugTraceChar=parent->debugTraceChar;
		debugTraceString=parent->debugTraceString;
		symbolPrepend=parent->symbolPrepend;
		isRoot=false;
	}
}
    

bool CompareEquals(APInt a,APInt b)
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

void CodeGenContext::StartFunctionDebugInfo(Function* func, YYLTYPE& declLoc)
{
	if (gContext.opts.generateDebug)
	{
		Type* retType = func->getReturnType();

		// Create function type information

		SmallVector<Metadata *, 16> EltTys;

		// Add the result type.
		if (retType->isVoidTy())
			EltTys.push_back(nullptr);
		else if (retType->isIntegerTy())
		{
			std::string name = "Bit" + to_string(retType->getIntegerBitWidth());
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
			std::string name = "Bit" + to_string(arg->getIntegerBitWidth());
			EltTys.push_back(dbgBuilder->createBasicType(name, arg->getIntegerBitWidth(), 0));
		}

		DISubroutineType* dbgFuncTy = dbgBuilder->createSubroutineType(dbgBuilder->getOrCreateTypeArray(EltTys));

		// Create function definition in debug information

		DIScope *FContext = gContext.scopingStack.top()->getFile();	// temporary - should come from the location information
		unsigned LineNo = declLoc.first_line;
		unsigned ScopeLine = LineNo;
		DISubprogram *SP = dbgBuilder->createFunction(
			FContext, SanitiseNameForDebug(func->getName()), StringRef(), gContext.scopingStack.top()->getFile(), LineNo,
			dbgFuncTy,
			false /* internal linkage */, true /* definition */, ScopeLine,
			DINode::FlagZero, false);
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
	std::map<std::string, std::map<APInt,std::string,myAPIntCompare> >::iterator tableIter;

	for (tableIter=disassemblyTable.begin();tableIter!=disassemblyTable.end();++tableIter)
	{
		APInt tableSize=tableIter->second.rbegin()->first;
		APInt tableSize32=tableSize.zextOrTrunc(32);
		// Create a global variable to indicate the max size of the table

		GlobalVariable* gvar_int32_DIS_max = new GlobalVariable(*module, IntegerType::get(TheContext, 32),true,GlobalValue::ExternalLinkage,nullptr,symbolPrepend+"DIS_max_"+tableIter->first);
		ConstantInt* const_int32_1 = ConstantInt::get(TheContext, tableSize32+1);
		gvar_int32_DIS_max->setInitializer(const_int32_1);

		// Create a global array to hold the table
		PointerType* PointerTy_5 = PointerType::get(IntegerType::get(TheContext, 8), 0);
	       	ArrayType* ArrayTy_4 = ArrayType::get(PointerTy_5, tableSize.getLimitedValue()+1);
		ConstantPointerNull* const_ptr_13 = ConstantPointerNull::get(PointerTy_5);	
		GlobalVariable* gvar_array_table = new GlobalVariable(*module,ArrayTy_4,true,GlobalValue::ExternalLinkage,nullptr, symbolPrepend+"DIS_"+tableIter->first);
		std::vector<Constant*> const_array_9_elems;

		std::map<APInt,std::string,myAPIntCompare>::iterator slot=tableIter->second.begin();
		APInt trackingSlot(tableSize.getBitWidth(),"0",16);

		while (slot!=tableIter->second.end())
		{
			if (CompareEquals(slot->first,trackingSlot))
			{
				ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(TheContext, 8), slot->second.length()-1);
				GlobalVariable* gvar_array__str = new GlobalVariable(*module, ArrayTy_0,true,GlobalValue::PrivateLinkage,0,symbolPrepend+".str"+trackingSlot.toString(16,false));
				gvar_array__str->setAlignment(1);
  
				Constant* const_array_9 = ConstantDataArray::getString(TheContext, slot->second.substr(1,slot->second.length()-2), true);
				std::vector<Constant*> const_ptr_12_indices;
				ConstantInt* const_int64_13 = ConstantInt::get(TheContext, APInt(64, StringRef("0"), 10));
				const_ptr_12_indices.push_back(const_int64_13);
				const_ptr_12_indices.push_back(const_int64_13);
				Constant* const_ptr_12 = ConstantExpr::getGetElementPtr(nullptr,gvar_array__str, const_ptr_12_indices);

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

		Constant* const_array_9 = ConstantArray::get(ArrayTy_4, const_array_9_elems);
		gvar_array_table->setInitializer(const_array_9);
	}
}

/* Compile the AST into a module */
void CodeGenContext::generateCode(CBlock& root)
{
	errorFlagged = false;

	if (isRoot)
	{
		optlevel = gContext.opts.optimisationLevel;
		if (gContext.opts.symbolModifier)
		{
			symbolPrepend=gContext.opts.symbolModifier;
		}

		// Testing - setup a compile unit for our root module
		if (gContext.opts.generateDebug)
		{
			gContext.scopingStack.push(CreateNewDbgFile(gContext.opts.inputFile, dbgBuilder));

			compileUnit = dbgBuilder->createCompileUnit(/*0x9999*/dwarf::DW_LANG_C99, gContext.scopingStack.top()->getFile(), "edlVxx", optlevel > 0, ""/*command line flags*/, 0);

#if defined(EDL_PLATFORM_MSVC)
			module->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
#else
			module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);
#endif
			module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
		}

		PointerType* PointerTy_4 = PointerType::get(IntegerType::get(TheContext, 8), 0);

		std::vector<Type*>FuncTy_8_args;
		FuncTy_8_args.push_back(PointerTy_4);
		FunctionType* FuncTy_8 = FunctionType::get(/*Result=*/IntegerType::get(TheContext, 32),/*Params=*/FuncTy_8_args,/*isVarArg=*/false);

		debugTraceString = Function::Create(/*Type=*/FuncTy_8,/*Linkage=*/GlobalValue::ExternalLinkage,/*Name=*/"puts", module); // (external, no body)
		debugTraceString->setCallingConv(CallingConv::C);

		std::vector<Type*>FuncTy_6_args;
		FuncTy_6_args.push_back(IntegerType::get(TheContext, 32));
		FunctionType* FuncTy_6 = FunctionType::get(/*Result=*/IntegerType::get(TheContext, 32),/*Params=*/FuncTy_6_args,/*isVarArg=*/false);

		debugTraceChar = Function::Create(/*Type=*/FuncTy_6,/*Linkage=*/GlobalValue::ExternalLinkage,/*Name=*/"putchar", module); // (external, no body)
		debugTraceChar->setCallingConv(CallingConv::C);
		
		debugTraceMissing = Function::Create(FuncTy_6,GlobalValue::ExternalLinkage,symbolPrepend+"missing", module); // (external, no body)
		debugTraceMissing->setCallingConv(CallingConv::C);
		
		std::vector<Type*>FuncTy_9_args;
		FuncTy_9_args.push_back(IntegerType::get(TheContext, 8));	//bitWidth (max 32)
		FuncTy_9_args.push_back(IntegerType::get(TheContext, 32));	//value
		FuncTy_9_args.push_back(PointerType::get(IntegerType::get(TheContext, 8), 0));	// char* busname
		FuncTy_9_args.push_back(IntegerType::get(TheContext, 32));	//dataDecode Width
		FuncTy_9_args.push_back(IntegerType::get(TheContext, 8));	// 0 / 1 - if 1 indicates the last tap in a connection list
		FunctionType* FuncTy_9 = FunctionType::get(/*Result=*/Type::getVoidTy(TheContext),/*Params=*/FuncTy_9_args, false);

		debugBusTap = Function::Create(FuncTy_9,GlobalValue::ExternalLinkage,symbolPrepend+"BusTap", module); // (external, no body)
		debugBusTap->setCallingConv(CallingConv::C);
	}

	root.prePass(*this);	/* Run a pre-pass on the code */
	root.codeGen(*this);	/* Generate complete code - starting at no block (global space) */

	// Finalise disassembly arrays
	if (gContext.opts.generateDisassembly)
	{
		GenerateDisassmTables();
	}
	
	if (errorFlagged)
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

			NamedMDNode *IdentMetadata = module->getOrInsertNamedMetadata("llvm.ident");
			std::string Version = "EDLvxx";
			LLVMContext &Ctx = module->getContext();

			Metadata *IdentNode[] = { llvm::MDString::get(Ctx, Version) };
			IdentMetadata->addOperand(llvm::MDNode::get(Ctx, IdentNode));
		}
		module->setDataLayout(ee->getDataLayout());
		module->setTargetTriple(sys::getDefaultTargetTriple()/*"x86_64--windows-msvc"*/);	// required for codeview debug information to be output AT ALL (need to update object file output)

		/* Print the bytecode in a human-readable format 
		   to see if our program compiled properly
		   */
		legacy::PassManager pm;

		pm.add(createVerifierPass());
		
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
				pm.add(createIPSCCPPass());

				// Now that we internalized some globals, see if we can hack on them!
				pm.add(createGlobalOptimizerPass());

				// Linking modules together can lead to duplicated global constants, only
				// keep one copy of each constant...
				pm.add(createConstantMergePass());

				// Remove unused arguments from functions...
				pm.add(createDeadArgEliminationPass());

				// Reduce the code after globalopt and ipsccp.  Both can open up significant
				// simplification opportunities, and both can propagate functions through
				// function pointers.  When this happens, we often have to resolve varargs
				// calls, etc, so let instcombine do this.
				pm.add(createInstructionCombiningPass());
				//pm.add(createFunctionInliningPass()); // Inline small functions
				pm.add(createPruneEHPass());              // Remove dead EH info
				pm.add(createGlobalDCEPass());            // Remove dead functions

				// If we didn't decide to inline a function, check to see if we can
				// transform it to pass arguments by value instead of by reference.
				pm.add(createArgumentPromotionPass());

				// The IPO passes may leave cruft around.  Clean up after them.
				pm.add(createInstructionCombiningPass());
				pm.add(createJumpThreadingPass());        // Thread jumps.
//				pm.add(createScalarReplAggregatesPass()); // Break up allocas

				// Run a few AA driven optimizations here and now, to cleanup the code.
				//pm.add(createFunctionAttrsPass());        // Add nocapture
				//pm.add(createGlobalsModRefPass());        // IP alias analysis
				pm.add(createLICMPass());                 // Hoist loop invariants
				pm.add(createGVNPass());                  // Remove common subexprs
				pm.add(createMemCpyOptPass());            // Remove dead memcpy's
				pm.add(createDeadStoreEliminationPass()); // Nuke dead stores

				// Cleanup and simplify the code after the scalar optimizations.
				pm.add(createInstructionCombiningPass());
				pm.add(createJumpThreadingPass());        // Thread jumps.
				pm.add(createPromoteMemoryToRegisterPass()); // Cleanup after threading.


				// Delete basic blocks, which optimization passes may have killed...
				pm.add(createCFGSimplificationPass());

				// Now that we have optimized the program, discard unreachable functions...
				pm.add(createGlobalDCEPass());
				pm.add(createCFGSimplificationPass());    // Clean up disgusting code
				pm.add(createPromoteMemoryToRegisterPass());// Kill useless allocas
				pm.add(createGlobalOptimizerPass());      // Optimize out global vars
				pm.add(createGlobalDCEPass());            // Remove unused fns and globs
				pm.add(createIPConstantPropagationPass());// IP Constant Propagation
				pm.add(createDeadArgEliminationPass());   // Dead argument elimination
				pm.add(createInstructionCombiningPass()); // Clean up after IPCP & DAE
				pm.add(createCFGSimplificationPass());    // Clean up after IPCP & DAE

				pm.add(createPruneEHPass());              // Remove dead EH info
				//pm.add(createFunctionAttrsPass());        // Deduce function attrs

				//  if (!DisableInline)
				pm.add(createFunctionInliningPass());   // Inline small functions
				pm.add(createArgumentPromotionPass());    // Scalarize uninlined fn args

				pm.add(createInstructionCombiningPass()); // Cleanup for scalarrepl.
				pm.add(createJumpThreadingPass());        // Thread jumps.
				pm.add(createCFGSimplificationPass());    // Merge & remove BBs
//				pm.add(createScalarReplAggregatesPass()); // Break up aggregate allocas
				pm.add(createInstructionCombiningPass()); // Combine silly seq's

				pm.add(createTailCallEliminationPass());  // Eliminate tail calls
				pm.add(createCFGSimplificationPass());    // Merge & remove BBs
				pm.add(createReassociatePass());          // Reassociate expressions
				pm.add(createLoopRotatePass());
				pm.add(createLICMPass());                 // Hoist loop invariants
				pm.add(createLoopUnswitchPass());         // Unswitch loops.
				// FIXME : Removing instcombine causes nestedloop regression.
				pm.add(createInstructionCombiningPass());
				pm.add(createIndVarSimplifyPass());       // Canonicalize indvars
				pm.add(createLoopDeletionPass());         // Delete dead loops
				pm.add(createLoopUnrollPass());           // Unroll small loops
				pm.add(createInstructionCombiningPass()); // Clean up after the unroller
				pm.add(createGVNPass());                  // Remove redundancies
				pm.add(createMemCpyOptPass());            // Remove memcpy / form memset
				pm.add(createSCCPPass());                 // Constant prop with SCCP

				// Run instcombine after redundancy elimination to exploit opportunities
				// opened up by them.
				pm.add(createInstructionCombiningPass());

				pm.add(createDeadStoreEliminationPass()); // Delete dead stores
				pm.add(createAggressiveDCEPass());        // Delete dead instructions
				pm.add(createCFGSimplificationPass());    // Merge & remove BBs
				pm.add(createStripDeadPrototypesPass());  // Get rid of dead prototypes
	//			pm.add(createDeadTypeEliminationPass());  // Eliminate dead types
				pm.add(createConstantMergePass());        // Merge dup global constants

				// Make sure everything is still good.
				pm.add(createVerifierPass());
			}
		}
		if (gContext.opts.outputFile != nullptr)
		{
			std::string error;
			auto triple = sys::getDefaultTargetTriple();
			auto target = TargetRegistry::lookupTarget(triple,error);
			if (!target)
			{
				errorFlagged = true;
				cerr << error << endl;
				return;
			}
			TargetOptions opt;
			auto rm = Optional<Reloc::Model>();
			auto targetMachine = target->createTargetMachine(triple, "generic", "", opt, rm);
			std::error_code ec;
			raw_fd_ostream dest(gContext.opts.outputFile, ec,llvm::sys::fs::F_None);
			if (ec)
			{
				errorFlagged = true;
				cerr << ec.message() << endl;
				return;
			}
			if (targetMachine->addPassesToEmitFile(pm, dest, TargetMachine::CGFT_ObjectFile))
			{
				errorFlagged = true;
				cerr << "Cannot emit object file" << endl;
				return;
			}
			pm.run(*module);
			dest.flush();
		}
		else
		{
			pm.add(createPrintModulePass(outs()));
			pm.run(*module);
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
						errorFlagged = true;
						return false;
					}
				}
				else
				{
					PrintErrorFromLocation(CombineTokenLocations(nameLoc,modLoc), "undeclared variable %s%s", module.c_str(), name.c_str());
					errorFlagged = true;
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
				errorFlagged = true;
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

Function* CodeGenContext::LookupFunctionInExternalModule(const std::string& moduleName, const std::string& name)
{
	Function* function = m_includes[moduleName]->module->getFunction(moduleName+name);

	return function;
}

CIdentifier CAliasDeclaration::empty("");

std::string stringZero("0");

CInteger CCastOperator::begZero(stringZero);

Value* CStateDeclaration::codeGen(CodeGenContext& context,Function* parent)
{
	// We need to create a bunch of labels - one for each case.. we will need a finaliser for the blocks when we pop out of our current block.
	entry = BasicBlock::Create(TheContext,id.name+"entry",parent);
	exit = BasicBlock::Create(TheContext,id.name+"exit",parent);

	return nullptr;
}

void CStatesDeclaration::prePass(CodeGenContext& context)
{
	int a;

	for (a=0;a<states.size();a++)
	{
		children.push_back(nullptr);
	}

	context.pushState(this);

	block.prePass(context);

	context.popState();

	if (context.currentIdent()!=nullptr)
	{
		int stateIndex = context.currentState()->getStateDeclarationIndex(*context.currentIdent());

		if (stateIndex!=-1)
		{
			context.currentState()->children[stateIndex]=this;
		}
	}
}

int GetNumStates(CStatesDeclaration* state)
{
	int a;
	int totalStates=0;

	if (state==nullptr)
		return 1;

	for (a=0;a<state->states.size();a++)
	{
		CStatesDeclaration* downState=state->children[a];

		if (downState==nullptr)
		{
			totalStates++;
		}
		else
		{
			totalStates+=GetNumStates(downState);
		}
	}

	return totalStates;
}
		
int ComputeBaseIdx(CStatesDeclaration* cur,CStatesDeclaration* find)
{
	int a;
	int totalStates=0;
	static bool found = false;

	if (cur==nullptr)
	{
		return 0;
	}

	found=false;

	for (a=0;a<cur->states.size();a++)
	{
		CStatesDeclaration* downState=cur->children[a];

		if (downState == find)
		{
			found=true;
			return totalStates;
		}

		if (downState==nullptr)
		{
			totalStates++;
		}
		else
		{
			totalStates+=ComputeBaseIdx(downState,find);
			if (found)
			{
				return totalStates;
			}
		}
	}

	return totalStates;
}

Value* CStatesDeclaration::codeGen(CodeGenContext& context)
{
	bool TopMostState=false;

	if (context.currentState()==nullptr)
	{
		if (context.parentHandler==nullptr)
		{
			PrintErrorFromLocation(statementLoc,"It is illegal to declare STATES outside of a handler");
			context.errorFlagged=true;
			return nullptr;
		}

		TopMostState=true;
	}
	else
	{
	}

	int totalStates;
	std::string numStates;
	APInt overSized;
	unsigned bitsNeeded;
	int baseStateIdx;
	
	Value *curState;
	Value *nxtState;
	std::string stateLabel = "STATE" + context.stateLabelStack;

	Value* int32_5=nullptr;
	if (TopMostState)
	{
		totalStates = GetNumStates(this);
		Twine numStatesTwine(totalStates);
		numStates = numStatesTwine.str();
		overSized=APInt(4*numStates.length(),numStates,10);
		bitsNeeded = overSized.getActiveBits();
		baseStateIdx=0;

		std::string curStateLbl = "CUR" + context.stateLabelStack;
		std::string nxtStateLbl = "NEXT" + context.stateLabelStack;
		std::string idxStateLbl = "IDX" + context.stateLabelStack;
		std::string stkStateLbl = "STACK" + context.stateLabelStack;

		GlobalVariable* gcurState = new GlobalVariable(*context.module,Type::getIntNTy(TheContext,bitsNeeded), false, GlobalValue::PrivateLinkage,nullptr,context.symbolPrepend+curStateLbl);
		GlobalVariable* gnxtState = new GlobalVariable(*context.module,Type::getIntNTy(TheContext,bitsNeeded), false, GlobalValue::PrivateLinkage,nullptr,context.symbolPrepend+nxtStateLbl);

		curState = gcurState;
		nxtState = gnxtState;

		ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(TheContext, bitsNeeded), MAX_SUPPORTED_STACK_DEPTH);
		GlobalVariable* stkState = new GlobalVariable(*context.module, ArrayTy_0, false,  GlobalValue::PrivateLinkage, nullptr, context.symbolPrepend+stkStateLbl);
		GlobalVariable *stkStateIdx = new GlobalVariable(*context.module,Type::getIntNTy(TheContext,MAX_SUPPORTED_STACK_BITS), false, GlobalValue::PrivateLinkage,nullptr,context.symbolPrepend+idxStateLbl);

		StateVariable newStateVar;
		newStateVar.currentState = curState;
		newStateVar.nextState = nxtState;
		newStateVar.stateStackNext = stkState;
		newStateVar.stateStackIndex = stkStateIdx;
		newStateVar.decl = this;

		context.states()[stateLabel]=newStateVar;
		context.statesAlt()[this]=newStateVar;

		// Constant Definitions
		ConstantInt* const_int32_n = ConstantInt::get(TheContext, APInt(bitsNeeded, 0, false));
		ConstantInt* const_int4_n = ConstantInt::get(TheContext, APInt(MAX_SUPPORTED_STACK_BITS, 0, false));
		ConstantAggregateZero* const_array_n = ConstantAggregateZero::get(ArrayTy_0);

		// Global Variable Definitions
		gcurState->setInitializer(const_int32_n);
		gnxtState->setInitializer(const_int32_n);
		stkStateIdx->setInitializer(const_int4_n);
		stkState->setInitializer(const_array_n);
	}
	else
	{
		StateVariable topState = context.states()[stateLabel];

		curState=topState.currentState;
		nxtState=topState.nextState;

		totalStates = GetNumStates(topState.decl);
		Twine numStatesTwine(totalStates);
		numStates = numStatesTwine.str();
		overSized=APInt(4*numStates.length(),numStates,10);
		bitsNeeded = overSized.getActiveBits();

		baseStateIdx=ComputeBaseIdx(topState.decl,this);

		int32_5 = topState.decl->optocurState;
	}
	BasicBlock* bb = context.currentBlock();		// cache old basic block

	std::map<std::string,BitVariable> tmp = context.locals();

	// Setup exit from switch statement
	exitState = BasicBlock::Create(TheContext,"switchTerm",bb->getParent());
	context.setBlock(exitState);

	// Step 1, load next state into current state
	if (TopMostState)
	{
		LoadInst* getNextState = new LoadInst(nxtState,"",false,bb);
		StoreInst* storeState = new StoreInst(getNextState,curState,false,bb);
	}

	// Step 2, generate switch statement
	if (TopMostState)
	{
		optocurState = new LoadInst(curState, "", false, bb);
		int32_5 = optocurState;
	}
	SwitchInst* void_6 = SwitchInst::Create(int32_5, exitState,totalStates, bb);


	// Step 3, build labels and initialiser for next state on entry
	bool lastStateAutoIncrement=true;
	int startOfAutoIncrementIdx=baseStateIdx;
	int startIdx=baseStateIdx;
	for (int a=0;a<states.size();a++)
	{
		// Build Labels
		int total = GetNumStates(children[a]);

		states[a]->codeGen(context,bb->getParent());
		for (int b=0;b<total;b++)
		{
			//
			ConstantInt* tt = ConstantInt::get(TheContext,APInt(bitsNeeded,startIdx+b,false));
			void_6->addCase(tt,states[a]->entry);
		}
		if (!lastStateAutoIncrement)
		{
			startOfAutoIncrementIdx=startIdx;
		}
		startIdx+=total;

		// Compute next state and store for future
		if (states[a]->autoIncrement)
		{
			if (children[a]==nullptr)
			{
				ConstantInt* nextState = ConstantInt::get(TheContext,APInt(bitsNeeded, a==states.size()-1 ? baseStateIdx : startIdx,false));
				StoreInst* newState = new StoreInst(nextState,nxtState,false,states[a]->entry);
			}
		}
		else
		{
			if (children[a]==nullptr)
			{
				ConstantInt* nextState = ConstantInt::get(TheContext,APInt(bitsNeeded, startOfAutoIncrementIdx,false));
				StoreInst* newState = new StoreInst(nextState,nxtState,false,states[a]->entry);
			}
		}

		lastStateAutoIncrement=states[a]->autoIncrement;
	}

	// Step 4, run code blocks to generate states
	context.pushState(this);
	block.codeGen(context);
	context.popState();

	// Finally we need to terminate the final blocks
	for (int a=0;a<states.size();a++)
	{
		BranchInst::Create(states[a]->exit,states[a]->entry);
		BranchInst::Create(exitState,states[a]->exit);			// this terminates the final blocks from our states
	}
	
	return nullptr;
}

void CStateDefinition::prePass(CodeGenContext& context)
{
	CStatesDeclaration* pStates = context.currentState();
	if (pStates)
	{
		CStateDeclaration* pState = pStates->getStateDeclaration(id);

		if (pState)
		{
			context.pushIdent(&id);
			block.prePass(context);
			context.popIdent();
		}
	}
}

Value* CStateDefinition::codeGen(CodeGenContext& context)
{
	CStatesDeclaration* pStates = context.currentState();
	if (pStates)
	{
		CStateDeclaration* pState = pStates->getStateDeclaration(id);

		if (pState)
		{
			context.pushBlock(pState->entry,block.blockStartLoc);
			block.codeGen(context);
			pState->entry=context.currentBlock();
			context.popBlock(block.blockEndLoc);
			return nullptr;
		}
		else
		{
			PrintErrorFromLocation(id.nameLoc, "Attempt to define unknown state");
			context.errorFlagged=true;
			return nullptr;
		}
	}
	else
	{
		PrintErrorFromLocation(id.nameLoc, "Attempt to define state entry when no states on stack");
		context.errorFlagged=true;
		return nullptr;
	}
}


void CHandlerDeclaration::prePass(CodeGenContext& context)
{
	block.prePass(context);
}

Value* CHandlerDeclaration::codeGen(CodeGenContext& context)
{
	vector<Type*> argTypes;
	BitVariable pinVariable;

	if (context.globals().find(id.name) == context.globals().end())
	{
		PrintErrorFromLocation(id.nameLoc,"undeclared pin %s - Handlers MUST be tied to pin definitions",id.name.c_str());
		context.errorFlagged=true;
		return nullptr;
	}

	pinVariable=context.globals()[id.name];

	if (pinVariable.writeAccessor==nullptr)
	{
		PrintErrorFromLocation(id.nameLoc,"Handlers must be tied to Input / Bidirectional pins ONLY");
		context.errorFlagged=true;
		return nullptr;
	}

	FunctionType *ftype = FunctionType::get(Type::getVoidTy(TheContext),argTypes, false);
	Function* function = Function::Create(ftype, GlobalValue::PrivateLinkage, context.moduleName + context.symbolPrepend+"HANDLER."+id.name, context.module);
	function->setDoesNotThrow();

	context.StartFunctionDebugInfo(function, handlerLoc);

	BasicBlock *bblock = BasicBlock::Create(TheContext, "entry", function, 0);
	
	context.m_handlers[id.name]=this;

	context.pushBlock(bblock,block.blockStartLoc);

	std::string oldStateLabelStack = context.stateLabelStack;
	context.stateLabelStack+="." + id.name;

	context.parentHandler=this;

	trigger.codeGen(context,pinVariable,function);

	block.codeGen(context);

	context.parentHandler=nullptr;

	Instruction* I=ReturnInst::Create(TheContext, context.currentBlock());			/* block may well have changed by time we reach here */
	if (context.gContext.opts.generateDebug)
	{
		I->setDebugLoc(DebugLoc::get(block.blockEndLoc.first_line, block.blockEndLoc.first_column, context.gContext.scopingStack.top()));
	}

	context.stateLabelStack=oldStateLabelStack;

	context.popBlock(block.blockEndLoc);

	context.EndFunctionDebugInfo();

	return function;
}
	
CString& CInstruction::processMnemonic(CodeGenContext& context,CString& in,llvm::APInt& opcode,unsigned num)
{
	static CString temp("");

	temp.quoted="";

	const char* ptr = in.quoted.c_str();

	int state=0;
	while (*ptr)
	{
		switch (state)
		{
		case 0:
			if (*ptr == '%')
			{
				state=1;
			}
			else
			{
				temp.quoted+=*ptr;
			}
			break;
		case 1:
			if (*ptr == 'M')
			{
				state=2;
				break;
			}
			if (*ptr == '$')
			{
				state=3;
				break;
			}

			state=0;
			break;
		case 2:
			{
				unsigned offset=*ptr-'0';
				const CString* res=operands[0]->GetString(context,num,offset);
				if (res)
				{
					temp.quoted+=res->quoted.substr(1,res->quoted.length()-2);
				}
				state=0;
			}
			break;
		case 3:
			// for now simply output the reference as %ref (debug harness will have to deal with these references (which should be pc + refnum)
			{
				temp.quoted+='%';
				temp.quoted+=*ptr;
				state=0;
			}
			break;
		}
		ptr++;
	}

	return temp;
}

CIdentifier CInstruction::emptyTable("");
CIdentifier CExecute::emptyTable("");

void CInstruction::prePass(CodeGenContext& context)
{
	block.prePass(context);
}

std::string EscapeString(const std::string &in)
{
	const char okChars[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
	std::string out="";

	for (int a=0;a<in.length();a++)
	{
		if (strchr(okChars,in[a])==nullptr)
		{
			out+='_';
		}
		else
		{
			out+=in[a];
		}
	}

	return out;
}

Value* CInstruction::codeGen(CodeGenContext& context)
{
	vector<Type*> argTypes;

	// First up, get the first operand (this must be a computable constant!) - remaining operands are only used for asm/disasm generation

	unsigned numOpcodes = operands[0]->GetNumComputableConstants(context);
	if (numOpcodes==0)
	{
		PrintErrorWholeLine(statementLoc,"Opcode for instruction must be able to generate constants");
		context.errorFlagged=true;
		return nullptr;
	}
			
//	std::cout << "Adding ins " << numOpcodes << std::endl;

	for (unsigned a=0;a<numOpcodes;a++)
	{
		APInt opcode = operands[0]->GetComputableConstant(context,a);

		CString disassembled=processMnemonic(context,mnemonic,opcode,a);
		CDebugTraceString opcodeString(disassembled);

		context.disassemblyTable[table.name][opcode]=disassembled.quoted;

		FunctionType *ftype = FunctionType::get(Type::getVoidTy(TheContext),argTypes, false);
		Function* function = Function::Create(ftype, GlobalValue::PrivateLinkage, EscapeString(context.symbolPrepend+"OPCODE_"+opcodeString.string.quoted.substr(1,opcodeString.string.quoted.length()-2) + "_" + table.name+opcode.toString(16,false)),context.module);
		function->setDoesNotThrow();

		context.StartFunctionDebugInfo(function, statementLoc);

		BasicBlock *bblock = BasicBlock::Create(TheContext, "entry", function, 0);

		context.pushBlock(bblock,block.blockStartLoc);

		operands[0]->DeclareLocal(context,a);

		block.codeGen(context);

		ReturnInst::Create(TheContext, context.currentBlock());			/* block may well have changed by time we reach here */

		context.popBlock(block.blockEndLoc);

		context.EndFunctionDebugInfo();

		if (context.errorFlagged)
		{
			return nullptr;
		}

		// Glue callee back into execute (assumes execute comes before instructions at all times for now)
		for (int b=0;b<context.executeLocations[table.name].size();b++)
		{
//			std::cout << "Adding execute " << opcode.toString(2,false) << std::endl;
			Function *parentFunction = context.executeLocations[table.name][b].blockEndForExecute->getParent();
			context.gContext.scopingStack.push(parentFunction->getSubprogram());
				
			BasicBlock* tempBlock = BasicBlock::Create(TheContext,"callOut" + table.name + opcode.toString(16,false),context.executeLocations[table.name][b].blockEndForExecute->getParent(),0);
			std::vector<Value*> args;
			CallInst* fcall = CallInst::Create(function,args,"",tempBlock);
			if (context.gContext.opts.generateDebug)
			{
				fcall->setDebugLoc(DebugLoc::get(context.executeLocations[table.name][b].executeLoc.first_line, context.executeLocations[table.name][b].executeLoc.first_column, context.gContext.scopingStack.top()));
			}
			BranchInst::Create(context.executeLocations[table.name][b].blockEndForExecute,tempBlock);
			context.executeLocations[table.name][b].switchForExecute->addCase(ConstantInt::get(TheContext,opcode),tempBlock);

			context.gContext.scopingStack.pop();
		}

	}
	return nullptr;
}

void CExecute::prePass(CodeGenContext& context)
{
	// this will allow us to have instructions before execute and vice versa
}

Value* CExecute::codeGen(CodeGenContext& context)
{
	Value* load = opcode.codeGen(context);
	
	if (load->getType()->isIntegerTy())
	{
		const IntegerType* typeForExecute = cast<IntegerType>(load->getType());

		ExecuteInformation temp;

		temp.executeLoc = executeLoc;
		temp.blockEndForExecute = BasicBlock::Create(TheContext, "execReturn", context.currentBlock()->getParent(), 0);		// Need to cache this block away somewhere
	
		if (context.gContext.opts.traceUnimplemented)
		{
			BasicBlock* tempBlock=BasicBlock::Create(TheContext,"default",context.currentBlock()->getParent(),0);

			std::vector<Value*> args;
	
			// Handle variable promotion
			Type* ty = Type::getIntNTy(TheContext,32);
			Instruction::CastOps op = CastInst::getCastOpcode(load,false,ty,false);

			Instruction* truncExt = CastInst::Create(op,load,ty,"cast",tempBlock);
		
			args.push_back(truncExt);
			CallInst *call = CallInst::Create(context.debugTraceMissing,args,"DEBUGTRACEMISSING",tempBlock);

			BranchInst::Create(temp.blockEndForExecute,tempBlock);
			
			temp.switchForExecute = SwitchInst::Create(load,tempBlock,2<<typeForExecute->getBitWidth(),context.currentBlock());
		}
		else
		{
			temp.switchForExecute = SwitchInst::Create(load,temp.blockEndForExecute,2<<typeForExecute->getBitWidth(),context.currentBlock());
		}

		context.setBlock(temp.blockEndForExecute);

		context.executeLocations[table.name].push_back(temp);
	}
	return nullptr;
}

int ComputeBaseIdx(CStatesDeclaration* cur,StateIdentList& list, int index,int &total)
{
	int a;
	int totalStates=0;
	static bool found = false;

	total=0;
	if (list.size()==1)
		return 0;

	for (a=0;a<cur->states.size();a++)
	{
		if (index && (cur->states[a]->id.name == list[index]->name))
		{
			CStatesDeclaration* downState=cur->children[a];
				
			if (index==list.size()-1)
			{
				found=true;
				if (downState==nullptr)
				{
					total=1;
				}
				else
				{
					total=ComputeBaseIdx(downState,list,0,total);
				}
				return totalStates;
			}

			if (downState==nullptr)
			{
				totalStates++;
			}
			else
			{
				int numStates=ComputeBaseIdx(downState,list,index+1,total);
				totalStates+=numStates;
				if (found)
				{
					return totalStates;
				}
			}
		}
		else
		{
			CStatesDeclaration* downState=cur->children[a];

			if (downState==nullptr)
			{
				totalStates++;
			}
			else
			{
				totalStates+=ComputeBaseIdx(downState,list,0,total);
			}

		}
	}

	if (!index)
	{
		return totalStates;
	}
	return -1;
}


void CStateTest::prePass(CodeGenContext& context)
{
	
}

Value* CStateTest::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE."+stateIdents[0]->name;

	if (context.states().find(stateLabel)==context.states().end())
	{
		PrintErrorFromLocation(stateIdents[0]->nameLoc, "Unknown handler, can't look up state reference");
		context.errorFlagged=true;
		return nullptr;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates=GetNumStates(topState.decl);
	int totalInBlock;

	int jumpIndex=ComputeBaseIdx(topState.decl,stateIdents,1,totalInBlock);
	
	if (jumpIndex==-1)
	{
		return UndefinedStateError(stateIdents, context);
	}

	Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();

	// Load value from state variable, test against range of values
	Value* load = new LoadInst(topState.currentState, "", false, context.currentBlock());

	if (totalInBlock>1)
	{
		CmpInst* cmp = CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_UGE,load, ConstantInt::get(TheContext, APInt(bitsNeeded,jumpIndex)), "", context.currentBlock());
		CmpInst* cmp2 = CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_ULT,load, ConstantInt::get(TheContext, APInt(bitsNeeded,jumpIndex+totalInBlock)), "", context.currentBlock());
		return BinaryOperator::Create(Instruction::And,cmp,cmp2,"Combining",context.currentBlock());
	}
		
	// Load value from state variable, test against being equal to found index, jump on result
	return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,load, ConstantInt::get(TheContext, APInt(bitsNeeded,jumpIndex)), "", context.currentBlock());
}

void CStateJump::prePass(CodeGenContext& context)
{
	
}

Value* CStateJump::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE."+stateIdents[0]->name;

	if (context.states().find(stateLabel)==context.states().end())
	{
		PrintErrorFromLocation(stateIdents[0]->nameLoc, "Unknown handler, can't look up state reference");
		context.errorFlagged=true;
		return nullptr;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates=GetNumStates(topState.decl);
	int totalInBlock;

	int jumpIndex=ComputeBaseIdx(topState.decl,stateIdents,1,totalInBlock);
	
	if (jumpIndex==-1)
	{
		return UndefinedStateError(stateIdents,context);
	}

	Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();
	
	return new StoreInst(ConstantInt::get(TheContext,APInt(bitsNeeded,jumpIndex)),topState.nextState,false,context.currentBlock());
}

void CStatePush::prePass(CodeGenContext& context)
{
	
}


Value* CStatePush::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE."+stateIdents[0]->name;

	if (context.states().find(stateLabel)==context.states().end())
	{
		PrintErrorFromLocation(stateIdents[0]->nameLoc, "Unknown handler, can't look up state reference");
		context.errorFlagged=true;
		return nullptr;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates=GetNumStates(topState.decl);
	int totalInBlock;

	int jumpIndex=ComputeBaseIdx(topState.decl,stateIdents,1,totalInBlock);
	
	if (jumpIndex==-1)
	{
		return UndefinedStateError(stateIdents,context);
	}

	Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();
	
	Value* index= new LoadInst(topState.stateStackIndex,"stackIndex",false,context.currentBlock());
	std::vector<Value*> indices;
	ConstantInt* const_intn = ConstantInt::get(TheContext, APInt(bitsNeeded, StringRef("0"), 10));
	indices.push_back(const_intn);
	indices.push_back(index);
	Value* ref = GetElementPtrInst::Create(nullptr,topState.stateStackNext, indices,"stackPos",context.currentBlock());

	Value* curNext = new LoadInst(topState.nextState,"currentNext",false,context.currentBlock());

	new StoreInst(curNext,ref,false,context.currentBlock());			// Save current next point to stack

	Value* inc = BinaryOperator::Create(Instruction::Add,index,ConstantInt::get(TheContext,APInt(MAX_SUPPORTED_STACK_BITS,1)),"incrementIndex",context.currentBlock());
	new StoreInst(inc,topState.stateStackIndex,false,context.currentBlock());	// Save updated index

	// Set next state
	return new StoreInst(ConstantInt::get(TheContext,APInt(bitsNeeded,jumpIndex)),topState.nextState,false,context.currentBlock());
}
	
void CStatePop::prePass(CodeGenContext& context)
{
	
}


Value* CStatePop::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE."+ident.name;

	if (context.states().find(stateLabel)==context.states().end())
	{
		PrintErrorFromLocation(ident.nameLoc, "Unknown handler, can't look up state reference");
		context.errorFlagged=true;
		return nullptr;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates=GetNumStates(topState.decl);
	int totalInBlock;

	Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();

	// We need to pop from our stack and put next back
	Value* index= new LoadInst(topState.stateStackIndex,"stackIndex",false,context.currentBlock());
	Value* dec = BinaryOperator::Create(Instruction::Sub,index,ConstantInt::get(TheContext,APInt(MAX_SUPPORTED_STACK_BITS,1)),"decrementIndex",context.currentBlock());
	new StoreInst(dec,topState.stateStackIndex,false,context.currentBlock());	// store new stack index

	std::vector<Value*> indices;
	ConstantInt* const_intn = ConstantInt::get(TheContext, APInt(bitsNeeded, StringRef("0"), 10));
	indices.push_back(const_intn);
	indices.push_back(dec);
	Value* ref = GetElementPtrInst::Create(nullptr,topState.stateStackNext, indices,"stackPos",context.currentBlock());

	Value* oldNext = new LoadInst(ref,"oldNext",false,context.currentBlock());	// retrieve old next value

	return new StoreInst(oldNext,topState.nextState,false,context.currentBlock());	// restore next state to old value
}

void CIfStatement::prePass(CodeGenContext& context)
{
	block.prePass(context);
}

Value* CIfStatement::codeGen(CodeGenContext& context)
{
	BasicBlock *then = BasicBlock::Create(TheContext,"then",context.currentBlock()->getParent());
	BasicBlock *endif = BasicBlock::Create(TheContext,"endif",context.currentBlock()->getParent());

	Value* result = expr.codeGen(context);
	BranchInst::Create(then,endif,result,context.currentBlock());

	context.pushBlock(then,block.blockStartLoc);

	block.codeGen(context);
	BranchInst::Create(endif,context.currentBlock());

	context.popBlock(block.blockEndLoc);

	context.setBlock(endif);

	return nullptr;
}

void CMappingDeclaration::prePass(CodeGenContext& context)
{

}

Value* CMappingDeclaration::codeGen(CodeGenContext& context)
{
	if (context.m_mappings.find(ident.name)!=context.m_mappings.end())
	{
		PrintErrorDuplication(ident.nameLoc,context.m_mappings[ident.name]->ident.nameLoc,"Multiple declaration for mapping");
		context.errorFlagged=true;
		return nullptr;
	}
	context.m_mappings[ident.name]=this;

	return nullptr;
}

void CConnectDeclaration::prePass(CodeGenContext& context)
{

}

Value* CConnectDeclaration::codeGen(CodeGenContext& context)
{
  	std::vector<Type*> FuncTy_8_args;
	FunctionType* FuncTy_8;
	Function* func = nullptr;

	// 1 argument at present, same as the ident - contains a single bit
	FuncTy_8_args.push_back(IntegerType::get(TheContext, 1));

	FuncTy_8 = FunctionType::get(Type::getVoidTy(TheContext),FuncTy_8_args,false);

	func=Function::Create(FuncTy_8,GlobalValue::ExternalLinkage,context.moduleName + context.symbolPrepend+ident.name,context.module);
	func->setCallingConv(CallingConv::C);
	func->setDoesNotThrow();
	
	context.StartFunctionDebugInfo(func, statementLoc);
	
	context.m_externFunctions[ident.name] = func;
	g_connectFunctions[func] = func;

	BasicBlock *bblock = BasicBlock::Create(TheContext, "entry", func, 0);
	
	context.pushBlock(bblock,blockStartLoc);

	Function::arg_iterator args = func->arg_begin();
	while (args!=func->arg_end())
	{
		BitVariable temp;

		temp.size = 1;
		temp.trueSize = 1;
		temp.cnst = APInt(1,0);
		temp.mask = ~temp.cnst;
		temp.shft = APInt(1,0);
		temp.aliased = false;
		temp.mappingRef = false;
		temp.pinType=0;
		temp.writeAccessor=nullptr;
		temp.writeInput=nullptr;
		temp.priorValue=nullptr;
		temp.impedance=nullptr;
		temp.fromExternal=false;

		temp.value=&*args;
		temp.value->setName(ident.name);
		context.locals()[ident.name]=temp;
		args++;
	}

	// Quickly spin through connection list and find the last bus tap
	size_t lastTap = 0;
	for (size_t searchLastTap = 0; searchLastTap < connects.size(); searchLastTap++)
	{
		if (connects[searchLastTap]->hasTap)
		{
			lastTap = searchLastTap;
		}
	}

	// This needs a re-write
	// for now - if a nullptr occurs in the input list, it is considered to be an isolation resistor,
	//
	// if only one side of the bus is driving, both sides get the final value as inputs
	// if neither side is driving, both sides get PULLUP
	// if both sides are driving, left and right sides get their respective values as input
	//
	//  A   |  B
	// Z Z  | Z Z		1
	// 1 1  | 1 1   
	//      |
	// a Z  | Z Z		2
	// A A  | A A
	//
	// Z Z  | b Z		3
	// B B  | B B
	//
	// a Z  | b Z		4
	// A A  | B B
	//
	// We can probably model this as -- Combine A Bus - seperately Combine B Bus - Generate an isDrivingBus for both sides - then select as :
	//
	// ~IsDrivingA & ~IsDrivingB - assign A inputs with A and B inputs with B  (both will be FF, but doesn't matter)
	// IsDrivingA & ~IsDrivingB - assign all inputs with A (simply set B=A)
	// ~IsDrivingA & IsDrivingB - assign all inputs with B (simply set A=B)
	// IsDrivingA & IsDrivingB - assign A inputs with A and B inputs with B
	//
	// So simplfication should be											1		2		3		4
	// outputA=Generate Bus A output										 FF		.a		 FF		.a
	// outputB=Generate Bus B output										 FF		 FF		.b		.b
	// IF isDrivingA ^ IsDrivingB											false	true	true	false
	//		outputB = select isDrivingA outputA outputB								a		b
	//		outputA = select isDrivingB outputB outputA								a		b
	// assign A inputs with outputA
	// assign B inputs with outputB
	//
	//

	for (size_t a=0;a<connects.size();a++)
	{
		// First step, figure out number of buses

		int busCount = 1;	// will always be at least 1 bus, but ISOLATION can generate split buses
		for (size_t b = 0; b < connects[a]->connects.size(); b++)
		{
			if ((*connects[a]).connects[b] == nullptr)
			{
				busCount++;
			}
		}

		// Do a quick pass to build a list of Inputs and Outputs - Maintaining declaration order (perform some validation as we go)
		int *outCnt = new int[busCount];
		int *inCnt = new int[busCount];
		int *biCnt = new int[busCount];
		memset(outCnt, 0, busCount * sizeof(int));
		memset(inCnt, 0, busCount * sizeof(int));
		memset(biCnt, 0, busCount * sizeof(int));
		std::vector<CIdentifier*> *ins= new std::vector<CIdentifier*>[busCount];
		std::vector<CExpression*> *outs = new std::vector<CExpression*>[busCount];

		int curBus = 0;
		for (size_t b=0;b<connects[a]->connects.size();b++)
		{
			if ((*connects[a]).connects[b] == nullptr)
			{
				// bus split
				curBus++;
				continue;
			}
			if ((*connects[a]).connects[b]->IsIdentifierExpression())
			{
				CIdentifier* ident = (CIdentifier*)(*connects[a]).connects[b];

				BitVariable var;
				
				if (!context.LookupBitVariable(var,ident->module,ident->name,ident->modLoc,ident->nameLoc))
				{
					return nullptr;
				}
				else
				{
					switch (var.pinType)
					{
						case 0:
//							std::cerr << "Variable : Acts as Output " << ident->name << std::endl;
							outs[curBus].push_back(ident);
							outCnt[curBus]++;
							break;
						case TOK_IN:
//							std::cerr << "PIN : Acts as Input " << ident->name << std::endl;
							ins[curBus].push_back(ident);
							inCnt[curBus]++;
							break;
						case TOK_OUT:
//							std::cerr << "PIN : Acts as Output " << ident->name << std::endl;
							outs[curBus].push_back(ident);
							outCnt[curBus]++;
							break;
						case TOK_BIDIRECTIONAL:
//							std::cerr << "PIN : Acts as Input/Output " << ident->name << std::endl;
							ins[curBus].push_back(ident);
							outs[curBus].push_back(ident);
							biCnt[curBus]++;
							break;
					}
				}
			}
			else
			{
//				std::cerr << "Complex Expression : Acts as Output " << std::endl;
				outs[curBus].push_back((*connects[a]).connects[b]);
				outCnt[curBus]++;
			}
		}

		for (size_t checkBus = 0; checkBus <= curBus; checkBus++)
		{
			if (outCnt[checkBus] == 0 && inCnt[checkBus] == 0 && biCnt[checkBus] == 0)
			{
				PrintErrorWholeLine(connects[a]->statementLoc,"No possible routing! - no connections");
				context.errorFlagged = true;
				return nullptr;
			}

			if (outCnt[checkBus] == 0 && inCnt[checkBus] > 0 && biCnt[checkBus] == 0)
			{
				PrintErrorWholeLine(connects[a]->statementLoc,"No possible routing! - all connections are input");
				context.errorFlagged = true;
				return nullptr;
			}

			if (outCnt[checkBus] > 0 && inCnt[checkBus] == 0 && biCnt[checkBus] == 0)
			{
				PrintErrorWholeLine(connects[a]->statementLoc,"No possible routing! - all connections are output");
				context.errorFlagged = true;
				return nullptr;
			}
		}
//		std::cerr << "Total Connections : in " << inCnt << " , out " << outCnt << " , bidirect " << biCnt << std::endl;

		Value** busOutResult=new Value*[busCount];
		Value** busIsDrivingResult=new Value*[busCount];

		// Generate bus output signals (1 per bus, + HiZ indicator per bus)
		for (curBus = 0; curBus < busCount; curBus++)
		{
			busOutResult[curBus] = nullptr;
			busIsDrivingResult[curBus] = ConstantInt::get(TheContext, APInt(1, 0));	// 0 for not driving bus

			for (size_t o = 0; o < outs[curBus].size(); o++)
			{
				CIdentifier* ident;
				BitVariable var;

				if (!outs[curBus][o]->IsIdentifierExpression())
				{
					busIsDrivingResult[curBus] = ConstantInt::get(TheContext, APInt(1, 1));	// complex expression, always a bus driver
				}
				else
				{
					ident = (CIdentifier*)outs[curBus][o];

					if (!context.LookupBitVariable(var, ident->module, ident->name,ident->modLoc,ident->nameLoc))
					{
						return nullptr;
					}

					if (var.impedance == nullptr)
					{
						busIsDrivingResult[curBus] = ConstantInt::get(TheContext, APInt(1, 1));	// complex expression, always a bus driver
					}
					else
					{
						Value* fetchDriving = new LoadInst(var.impedance, "fetchImpedanceForIsolation", false, bblock);
						fetchDriving = CmpInst::Create(Instruction::ICmp, ICmpInst::ICMP_EQ, fetchDriving, ConstantInt::get(TheContext, APInt(var.size.getLimitedValue(), 0)), "isNotImpedance", bblock);

						Instruction* I = BinaryOperator::Create(Instruction::Or, fetchDriving, busIsDrivingResult[curBus], "isDriving", context.currentBlock());
						if (context.gContext.opts.generateDebug)
						{
							I->setDebugLoc(DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
						}
						busIsDrivingResult[curBus] = I;
					}
				}


				Value* tmp = outs[curBus][o]->codeGen(context);

				if (o == 0)
				{
					busOutResult[curBus] = tmp;
				}
				else
				{
					Type* lastType = busOutResult[curBus]->getType();
					Type* tmpType = tmp->getType();
					if (lastType != tmpType)
					{
						// Should be integer types at the point
						if (!lastType->isIntegerTy() || !tmpType->isIntegerTy())
						{
							PrintErrorWholeLine(connects[a]->statementLoc, "(TODO) Expected integer types");
							context.errorFlagged = true;
							return nullptr;
						}
						if (lastType->getPrimitiveSizeInBits() < tmpType->getPrimitiveSizeInBits())
						{
							// need cast last zeroextend
							busOutResult[curBus] = new ZExtInst(busOutResult[curBus], tmpType, "ZExt", context.currentBlock());
						}
						else
						{
							// need cast tmp zeroextend
							tmp = new ZExtInst(tmp, lastType, "ZExt", context.currentBlock());
						}
					}

					Instruction* I = BinaryOperator::Create(Instruction::And, tmp, busOutResult[curBus], "PullUpCombine", context.currentBlock());
					if (context.gContext.opts.generateDebug)
					{
						I->setDebugLoc(DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
					}
					busOutResult[curBus] = I;
				}
			}
		}

		// Output busses all generated - generate the code to fix up the buses as per the rules above - todo fix this for beyond 2 bus case

		if (busCount == 2)
		{
			Value* needsSwap =BinaryOperator::Create(Instruction::Xor, busIsDrivingResult[0], busIsDrivingResult[1], "RequiresSwap", context.currentBlock()); 
			Value* swapA = BinaryOperator::Create(Instruction::And, busIsDrivingResult[1], needsSwap, "setAtoB", context.currentBlock());
			Value* swapB = BinaryOperator::Create(Instruction::And, busIsDrivingResult[0], needsSwap, "setBtoA", context.currentBlock());

			{// extend type widths to match
				Type* lastType = busOutResult[0]->getType();
				Type* tmpType = busOutResult[1]->getType();
				if (lastType != tmpType)
				{
					// Should be integer types at the point
					if (!lastType->isIntegerTy() || !tmpType->isIntegerTy())
					{
						PrintErrorWholeLine(connects[a]->statementLoc, "(TODO) Expected integer types");
						context.errorFlagged = true;
						return nullptr;
					}
					if (lastType->getPrimitiveSizeInBits() < tmpType->getPrimitiveSizeInBits())
					{
						// need cast last zeroextend
						busOutResult[0] = new ZExtInst(busOutResult[0], tmpType, "ZExt", context.currentBlock());
					}
					else
					{
						// need cast tmp zeroextend
						busOutResult[1] = new ZExtInst(busOutResult[1], lastType, "ZExt", context.currentBlock());
					}
				}
			}


			Instruction* swapAB = SelectInst::Create(swapA, busOutResult[1], busOutResult[0], "SwapAwithB", context.currentBlock());
			Instruction* swapBA = SelectInst::Create(swapB, busOutResult[0], busOutResult[1], "SwapBwithA", context.currentBlock());
			if (context.gContext.opts.generateDebug)
			{
				swapAB->setDebugLoc(DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
				swapBA->setDebugLoc(DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
			}

			busOutResult[0] = swapAB;
			busOutResult[1] = swapBA;
		}
		if (busCount > 2)
		{
			PrintErrorWholeLine(connects[a]->statementLoc, "(TODO) Bus arbitration for >2 buses not implemented yet");
			context.errorFlagged = true;
			return nullptr;
		}

		for (curBus = 0; curBus < busCount; curBus++)
		{
			for (size_t i = 0; i < ins[curBus].size(); i++)
			{
				BitVariable var;

				if (!context.LookupBitVariable(var, ins[curBus][i]->module, ins[curBus][i]->name, ins[curBus][i]->modLoc, ins[curBus][i]->nameLoc))
				{
					return nullptr;
				}

				Instruction* I = CAssignment::generateAssignment(var, *ins[curBus][i], busOutResult[curBus], context);
				if (context.gContext.opts.generateDebug)
				{
					I->setDebugLoc(DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
				}

				if (curBus==0 && i == 0 && connects[a]->hasTap && optlevel < 3)		// only tap the first bus for now
				{
					// Generate a call to AddPinRecord(cnt,val,name,last)

					std::vector<Value*> args;

					// Create global string for bus tap name
					Value* nameRef=connects[a]->tapName.codeGen(context);

					// Handle decode width promotion
					Value* DW = connects[a]->decodeWidth.codeGen(context);
					Type* tyDW = Type::getIntNTy(TheContext, 32);
					Instruction::CastOps opDW = CastInst::getCastOpcode(DW, false, tyDW, false);

					Instruction* truncExtDW = CastInst::Create(opDW, DW, tyDW, "cast", context.currentBlock());


					// Handle variable promotion
					Type* ty = Type::getIntNTy(TheContext, 32);
					Instruction::CastOps op = CastInst::getCastOpcode(busOutResult[curBus], false, ty, false);

					Instruction* truncExt = CastInst::Create(op, busOutResult[curBus], ty, "cast", context.currentBlock());
					args.push_back(ConstantInt::get(TheContext, APInt(8, var.trueSize.getLimitedValue(), false)));
					args.push_back(truncExt);
					args.push_back(nameRef);
					args.push_back(truncExtDW);
					args.push_back(ConstantInt::get(TheContext, APInt(8, a==lastTap?1:0, false)));
					CallInst *call = CallInst::Create(context.debugBusTap, args, "", context.currentBlock());
				}
			}
		}
	}

	Instruction* I=ReturnInst::Create(TheContext, context.currentBlock());			/* block may well have changed by time we reach here */
	if (context.gContext.opts.generateDebug)
	{
		I->setDebugLoc(DebugLoc::get(blockEndLoc.first_line, blockEndLoc.first_column, context.gContext.scopingStack.top()));
	}

	context.popBlock(blockEndLoc);

	context.EndFunctionDebugInfo();

	return func;
}

CInteger CAffect::emptyParam(stringZero);

Value* CAffect::codeGenCarry(CodeGenContext& context,Value* exprResult,Value* lhs,Value* rhs,int optype)
{
	IntegerType* resultType = cast<IntegerType>(exprResult->getType());
	Value *answer;
	switch (type)
	{
		case TOK_CARRY:
		case TOK_NOCARRY:
			{
				if (param.integer.getLimitedValue() >= resultType->getBitWidth())
				{
					PrintErrorFromLocation(param.integerLoc,"Bit to carry is outside of range for result");
					context.errorFlagged=true;
					return nullptr;
				}
				
				IntegerType* lhsType = cast<IntegerType>(lhs->getType());
				IntegerType* rhsType = cast<IntegerType>(rhs->getType());
		       		
				Instruction::CastOps lhsOp = CastInst::getCastOpcode(lhs,false,resultType,false);
				lhs = CastInst::Create(lhsOp,lhs,resultType,"cast",context.currentBlock());
				Instruction::CastOps rhsOp = CastInst::getCastOpcode(rhs,false,resultType,false);
				rhs = CastInst::Create(rhsOp,rhs,resultType,"cast",context.currentBlock());

				if (optype==TOK_ADD || optype==TOK_DADD)
				{
					//((lh & rh) | ((~Result) & rh) | (lh & (~Result)))

					// ~Result
					Value* cmpResult = BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(TheContext,~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());

					// lh&rh
					Value* expr1 = BinaryOperator::Create(Instruction::And,rhs,lhs,"",context.currentBlock());
					// ~Result & rh
					Value* expr2 = BinaryOperator::Create(Instruction::And,cmpResult,lhs,"",context.currentBlock());
					// lh & ~Result
					Value* expr3 = BinaryOperator::Create(Instruction::And,rhs,cmpResult,"",context.currentBlock());

					// lh&rh | ~Result&rh
					Value* combine = BinaryOperator::Create(Instruction::Or,expr1,expr2,"",context.currentBlock());
					// (lh&rh | ~Result&rh) | lh&~Result
					answer = BinaryOperator::Create(Instruction::Or,combine,expr3,"",context.currentBlock());
				}
				else
				{
					//((~lh&Result) | (~lh&rh) | (rh&Result)
					
					// ~lh
					Value* cmpLhs = BinaryOperator::Create(Instruction::Xor,lhs,ConstantInt::get(TheContext,~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());

					// ~lh&Result
					Value* expr1 = BinaryOperator::Create(Instruction::And,cmpLhs,exprResult,"",context.currentBlock());
					// ~lh & rh
					Value* expr2 = BinaryOperator::Create(Instruction::And,cmpLhs,rhs,"",context.currentBlock());
					// rh & Result
					Value* expr3 = BinaryOperator::Create(Instruction::And,rhs,exprResult,"",context.currentBlock());

					// ~lh&Result | ~lh&rh
					Value* combine = BinaryOperator::Create(Instruction::Or,expr1,expr2,"",context.currentBlock());
					// (~lh&Result | ~lh&rh) | rh&Result
					answer = BinaryOperator::Create(Instruction::Or,combine,expr3,"",context.currentBlock());
				}
			}
			break;

		default:
			assert(0 && "Unknown affector");
			break;
	}

	if (tmpResult)
	{
		tmpResult=BinaryOperator::Create(Instruction::Or,answer,tmpResult,"",context.currentBlock());
	}
	else
	{
		tmpResult=answer;
	}

	return tmpResult;
}

Value* CAffect::codeGenFinal(CodeGenContext& context,Value* exprResult)
{
	BitVariable var;
	if (!context.LookupBitVariable(var,ident.module,ident.name,ident.modLoc,ident.nameLoc))
	{
		return nullptr;
	}
	
	if (var.mappingRef)
	{
		PrintErrorFromLocation(var.refLoc, "Cannot perform operation on a mapping reference");
		context.errorFlagged=true;
		return nullptr;
	}

	IntegerType* resultType = cast<IntegerType>(exprResult->getType());
	Value *answer;
	switch (type)
	{
		case TOK_FORCESET:
		case TOK_FORCERESET:
			{
				APInt bits(resultType->getBitWidth(),0,false);
				if (type==TOK_FORCESET)
				{
					bits = ~bits;
				}
				answer=ConstantInt::get(TheContext,bits);
			}
			break;
		case TOK_ZERO:
			answer=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,exprResult, ConstantInt::get(TheContext,APInt(resultType->getBitWidth(),0,false)), "", context.currentBlock());
			break;
		case TOK_NONZERO:
			answer=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_NE,exprResult, ConstantInt::get(TheContext,APInt(resultType->getBitWidth(),0,false)), "", context.currentBlock());
			break;
		case TOK_SIGN:
		case TOK_NOSIGN:
			{
				APInt signBit(resultType->getBitWidth(),0,false);
				signBit.setBit(resultType->getBitWidth()-1);
				if (type==TOK_SIGN)
				{
					answer=BinaryOperator::Create(Instruction::And,exprResult,ConstantInt::get(TheContext,signBit),"",context.currentBlock());
				}
				else
				{
					answer=BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(TheContext,signBit),"",context.currentBlock());
				}
				answer=BinaryOperator::Create(Instruction::LShr,answer,ConstantInt::get(TheContext,APInt(resultType->getBitWidth(),(uint64_t)(resultType->getBitWidth()-1),false)),"",context.currentBlock());
			}
			break;
		case TOK_PARITYODD:
		case TOK_PARITYEVEN:
			
			// To generate the parity of a number, we use the parallel method. minimum width is 4 bits then for each ^2 bit size we add an extra shift operation
			{
				APInt computeClosestPower2Size(3,"100",2);
				unsigned count=0;
				while (true)
				{
					if (resultType->getBitWidth() <= computeClosestPower2Size.getLimitedValue())
					{
						// Output a casting operator to up the size of type
						Type* ty = Type::getIntNTy(TheContext,computeClosestPower2Size.getLimitedValue());
						Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,ty,false);
						answer = CastInst::Create(op,exprResult,ty,"",context.currentBlock());
						break;
					}

					count++;
					computeClosestPower2Size=computeClosestPower2Size.zext(3+count);
					computeClosestPower2Size=computeClosestPower2Size.shl(1);			//TODO fix infinite loop if we ever create more bits than APInt can handle! 
				}

				// answer is now appropriate size, next step for each count shrink the bits so we can test a nibble

				computeClosestPower2Size=computeClosestPower2Size.zext(computeClosestPower2Size.getLimitedValue());
				computeClosestPower2Size=computeClosestPower2Size.lshr(1);
				for (int a=0;a<count;a++)
				{
					Value *shifted = BinaryOperator::Create(Instruction::LShr,answer,ConstantInt::get(TheContext,computeClosestPower2Size),"",context.currentBlock());
					answer=BinaryOperator::Create(Instruction::Xor,shifted,answer,"",context.currentBlock());
					computeClosestPower2Size=computeClosestPower2Size.lshr(1);
				}

				// final part, mask to nibble size and use this to lookup into magic constant 0x6996 (which is simply a table look up for the parities of a nibble)
				answer=BinaryOperator::Create(Instruction::And,answer,ConstantInt::get(TheContext,APInt(computeClosestPower2Size.getBitWidth(),0xF,false)),"",context.currentBlock());
				Type* ty = Type::getIntNTy(TheContext,16);
				Instruction::CastOps op = CastInst::getCastOpcode(answer,false,ty,false);
				answer = CastInst::Create(op,answer,ty,"",context.currentBlock());
				if (type == TOK_PARITYEVEN)
				{
					answer=BinaryOperator::Create(Instruction::LShr,ConstantInt::get(TheContext,~APInt(16,0x6996,false)),answer,"",context.currentBlock());
				}
				else
				{
					answer=BinaryOperator::Create(Instruction::LShr,ConstantInt::get(TheContext,APInt(16,0x6996,false)),answer,"",context.currentBlock());
				}
			}
			break;
		case TOK_BIT:
		case TOK_INVBIT:
			{
				APInt bit(resultType->getBitWidth(),0,false);
				APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
				bit.setBit(param.integer.getLimitedValue());
				if (type==TOK_BIT)
				{
					answer=BinaryOperator::Create(Instruction::And,exprResult,ConstantInt::get(TheContext,bit),"",context.currentBlock());
				}
				else
				{
					answer=BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(TheContext,bit),"",context.currentBlock());
				}
				answer=BinaryOperator::Create(Instruction::LShr,answer,ConstantInt::get(TheContext,shift),"",context.currentBlock());
			}
			break;
		case TOK_OVERFLOW:
		case TOK_NOOVERFLOW:
			{
				APInt bit(resultType->getBitWidth(),0,false);
				APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
				bit.setBit(param.integer.getLimitedValue());
				ConstantInt* bitC=ConstantInt::get(TheContext,bit);
				ConstantInt* shiftC=ConstantInt::get(TheContext,shift);

				if (param.integer.getLimitedValue() >= resultType->getBitWidth())
				{
					PrintErrorFromLocation(param.integerLoc,"Bit for overflow detection is outside of range for result");
					context.errorFlagged=true;
					return nullptr;
				}
				Value* lhs = ov1Val;		// Lhs and Rhs are provided in the overflow affector
				if (lhs==nullptr)
				{
					context.errorFlagged=true;
					return nullptr;
				}
				Value* rhs = ov2Val;
				if (rhs==nullptr)
				{
					context.errorFlagged=true;
					return nullptr;
				}
				
				IntegerType* lhsType = cast<IntegerType>(lhs->getType());
				IntegerType* rhsType = cast<IntegerType>(rhs->getType());
				
				if (param.integer.getLimitedValue() >= lhsType->getBitWidth())
				{
					PrintErrorFromLocation(param.integerLoc,"Bit for overflow detection is outside of range of lhs");
					std::cerr << "Bit for overflow detection is outside of range for source1" << std::endl;
					context.errorFlagged=true;
					return nullptr;
				}
				if (param.integer.getLimitedValue() >= rhsType->getBitWidth())
				{
					PrintErrorFromLocation(param.integerLoc,"Bit for overflow detection is outside of range of rhs");
					context.errorFlagged=true;
					return nullptr;
				}
		       		
				Instruction::CastOps lhsOp = CastInst::getCastOpcode(lhs,false,resultType,false);
				lhs = CastInst::Create(lhsOp,lhs,resultType,"cast",context.currentBlock());
				Instruction::CastOps rhsOp = CastInst::getCastOpcode(rhs,false,resultType,false);
				rhs = CastInst::Create(rhsOp,rhs,resultType,"cast",context.currentBlock());

				if (opType==TOK_ADD || opType==TOK_DADD)
				{
					//((rh & lh & (~Result)) | ((~rh) & (~lh) & Result))

					// ~Result
					Value* cmpResult = BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(TheContext,~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());
					// ~lh
					Value* cmplh = BinaryOperator::Create(Instruction::Xor,lhs,ConstantInt::get(TheContext,~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());
					// ~rh
					Value* cmprh = BinaryOperator::Create(Instruction::Xor,rhs,ConstantInt::get(TheContext,~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());

					// lh&rh
					Value* expr1 = BinaryOperator::Create(Instruction::And,rhs,lhs,"",context.currentBlock());
					// ~rh&~lh
					Value* expr2 = BinaryOperator::Create(Instruction::And,cmprh,cmplh,"",context.currentBlock());

					// rh & lh & ~Result
					Value* expr3 = BinaryOperator::Create(Instruction::And,expr1,cmpResult,"",context.currentBlock());
					// ~rh & ~lh & Result
					Value* expr4 = BinaryOperator::Create(Instruction::And,expr2,exprResult,"",context.currentBlock());

					// rh & lh & ~Result | ~rh & ~lh & Result
					answer = BinaryOperator::Create(Instruction::Or,expr3,expr4,"",context.currentBlock());
				}
				else
				{
					//(((~rh) & lh & (~Result)) | (rh & (~lh) & Result))
					
					// ~Result
					Value* cmpResult = BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(TheContext,~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());
					// ~lh
					Value* cmplh = BinaryOperator::Create(Instruction::Xor,lhs,ConstantInt::get(TheContext,~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());
					// ~rh
					Value* cmprh = BinaryOperator::Create(Instruction::Xor,rhs,ConstantInt::get(TheContext,~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());

					// ~rh&lh
					Value* expr1 = BinaryOperator::Create(Instruction::And,cmprh,lhs,"",context.currentBlock());
					// rh&~lh
					Value* expr2 = BinaryOperator::Create(Instruction::And,rhs,cmplh,"",context.currentBlock());
					
					// ~rh & lh & ~Result
					Value* expr3 = BinaryOperator::Create(Instruction::And,expr1,cmpResult,"",context.currentBlock());
					// rh & ~lh & Result
					Value* expr4 = BinaryOperator::Create(Instruction::And,expr2,exprResult,"",context.currentBlock());

					// ~rh & lh & ~Result | rh & ~lh & Result
					answer = BinaryOperator::Create(Instruction::Or,expr3,expr4,"",context.currentBlock());
				}
				if (type==TOK_OVERFLOW)
				{
					answer=BinaryOperator::Create(Instruction::And,answer,bitC,"",context.currentBlock());
				}
				else
				{
					answer=BinaryOperator::Create(Instruction::Xor,answer,bitC,"",context.currentBlock());
				}
				answer=BinaryOperator::Create(Instruction::LShr,answer,shiftC,"",context.currentBlock());
			}
			break;
		case TOK_CARRY:
		case TOK_NOCARRY:
			{
				APInt bit(resultType->getBitWidth(),0,false);
				APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
				bit.setBit(param.integer.getLimitedValue());
				ConstantInt* bitC=ConstantInt::get(TheContext,bit);
				ConstantInt* shiftC=ConstantInt::get(TheContext,shift);

				if (tmpResult==nullptr)
				{
					assert(0 && "Failed to compute CARRY/OVERFLOW for expression (possible bug in compiler)");
				}

				if (type==TOK_CARRY)
				{
					answer=BinaryOperator::Create(Instruction::And,tmpResult,bitC,"",context.currentBlock());
				}
				else
				{
					answer=BinaryOperator::Create(Instruction::Xor,tmpResult,bitC,"",context.currentBlock());
				}
				answer=BinaryOperator::Create(Instruction::LShr,answer,shiftC,"",context.currentBlock());
			}
			break;

		default:
			assert(0 && "Unknown affector");
			break;
	}

	return CAssignment::generateAssignment(var,ident,answer,context);
}

void CAffector::prePass(CodeGenContext& context)
{
	expr.prePass(context);
}

Value* CAffector::codeGen(CodeGenContext& context)
{
	Value* left=nullptr;
	Value* right=nullptr;
	Value* exprResult=nullptr;
	bool containsCarryOverflow=false;
	int type=0;

	if (context.curAffectors.size())
	{
		PrintErrorFromLocation(exprLoc, "(TODO) Cannot currently supply nested affectors");
		context.errorFlagged=true;
		return nullptr;
	}

	// If there is more than 1 expr that is used to compute CARRY/OVERFLOW, we need to test at each stage.
	
	for (int a=0;a<affectors.size();a++)
	{
		switch (affectors[a]->type)
		{
			case TOK_CARRY:
			case TOK_NOCARRY:
				containsCarryOverflow=true;
				affectors[a]->tmpResult=nullptr;
				context.curAffectors.push_back(affectors[a]);
				break;
			case TOK_OVERFLOW:
			case TOK_NOOVERFLOW:
				containsCarryOverflow=true;
				affectors[a]->tmpResult=nullptr;
				affectors[a]->ov1Val=affectors[a]->ov1.codeGen(context);
				affectors[a]->ov2Val=affectors[a]->ov2.codeGen(context);
				context.curAffectors.push_back(affectors[a]);
				break;
			default:
				break;
		}
	}

	if (containsCarryOverflow)
	{
		if (expr.IsLeaf() /*|| (!expr.IsCarryExpression())*/)
		{
			PrintErrorFromLocation(exprLoc, "OVERFLOW/CARRY is not supported for non carry/overflow expressions");
			context.errorFlagged=true;
			return nullptr;
		}
	}

	exprResult=expr.codeGen(context);

	if (exprResult)
	{
		// Final result is in exprResult (+affectors for carry/overflow).. do final operations
		for (int a = 0; a < affectors.size(); a++)
		{
			affectors[a]->codeGenFinal(context, exprResult);
		}
	}
	context.curAffectors.clear();

	return exprResult;
}

void CExternDecl::prePass(CodeGenContext& context)
{

}

Value* CExternDecl::codeGen(CodeGenContext& context)
{
	// Build up the function type parameters. At present the only allowed sizes are : 
	// 	0 -> void	(only allowed for return!!)
	// 	8 -> unsigned char  (technically u_int8_t)
	// 	16-> unsigned short (technically u_int16_t)
	// 	32-> unsigned int   (technically u_int32_t)

  	std::vector<Type*> FuncTy_8_args;
	for (int a=0;a<params.size();a++)
	{
		unsigned size = params[a]->integer.getLimitedValue();
		if (size!=8 && size!=16 && size!=32)
		{
			PrintErrorFromLocation(params[a]->integerLoc,"External C functions must use C size parameters (8,16 or 32 bits)");
			context.errorFlagged=true;
			return nullptr;
		}
		FuncTy_8_args.push_back(IntegerType::get(TheContext, size));
	}
	FunctionType* FuncTy_8;
	if (returns.empty())
	{
		FuncTy_8 = FunctionType::get(Type::getVoidTy(TheContext),FuncTy_8_args,false);
	}
	else
	{
		unsigned size = returns[0]->integer.getLimitedValue();
		if (size!=8 && size!=16 && size!=32)
		{
			PrintErrorFromLocation(returns[0]->integerLoc,"External C functions must use C size parameters (8,16 or 32 bits)");
			context.errorFlagged=true;
			return nullptr;
		}
		FuncTy_8 = FunctionType::get(IntegerType::get(TheContext, size),FuncTy_8_args,false);
	}

	Function* func = Function::Create(FuncTy_8,GlobalValue::ExternalLinkage,context.symbolPrepend+name.name,context.module);
	func->setCallingConv(CallingConv::C);

	context.m_externFunctions[name.name] = func;

	return nullptr;
}

void CFuncCall::prePass(CodeGenContext& context)
{
	for (int a=0,b=0;a<params.size();a++,b++)
	{
		params[a]->prePass(context);
	}
}

Value* CFuncCall::codeGen(CodeGenContext& context)
{
	if (context.m_externFunctions.find(name.name)==context.m_externFunctions.end())
	{
		PrintErrorFromLocation(name.nameLoc,"Function Declaration Required to use a Function");
		context.errorFlagged=true;
		return nullptr;
	}
	Function* func = context.m_externFunctions[name.name];

	std::vector<Value*> args;
	const FunctionType* funcType = func->getFunctionType();

	for (int a=0,b=0;a<params.size();a++,b++)
	{
		if (b==funcType->getNumParams())
		{
			PrintErrorWholeLine(name.nameLoc,"Wrong Number Of Arguments To Function");
			context.errorFlagged=true;
			return nullptr;
		}

		Value* exprResult = params[a]->codeGen(context);

		// We need to promote the return type to the same as the function parameters (Auto promote rules)
		Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,funcType->getParamType(b),false);
		exprResult = CastInst::Create(op,exprResult,funcType->getParamType(b),"",context.currentBlock());

		args.push_back(exprResult);
	}
	Value* callReturn;
	if (funcType->getReturnType()->isVoidTy())
	{
		Instruction* call = CallInst::Create(func, args, "", context.currentBlock());
		if (context.gContext.opts.generateDebug)
		{
			call->setDebugLoc(DebugLoc::get(callLoc.first_line, callLoc.first_column, context.gContext.scopingStack.top()));
		}

		callReturn = ConstantInt::get(TheContext,APInt(1,0));		// Forces void returns to actually return 0
	}
	else
	{
		Instruction* call = CallInst::Create(func, args, "CallingCFunc"+name.name, context.currentBlock());
		if (context.gContext.opts.generateDebug)
		{
			call->setDebugLoc(DebugLoc::get(callLoc.first_line, callLoc.first_column, context.gContext.scopingStack.top()));
		}
		callReturn = call;
	}

	return callReturn;
}

CInteger CVariableDeclaration::notArray(stringZero);
CInteger CTrigger::zero(stringZero);

Value* CTrigger::codeGen(CodeGenContext& context,BitVariable& pin,Value* function)
{
	switch (type)
	{
		case TOK_ALWAYS:
			{
				BasicBlock *oldBlock = (*pin.writeAccessor)->getParent();
				Function* parentFunction = oldBlock->getParent();
				context.gContext.scopingStack.push(parentFunction->getSubprogram());
				
				CallInst* fcall = CallInst::Create(function,"",*pin.writeAccessor);
				if (context.gContext.opts.generateDebug)
				{
					fcall->setDebugLoc(DebugLoc::get(debugLocation.first_line, debugLocation.first_column, context.gContext.scopingStack.top()));
				}

				context.gContext.scopingStack.pop();
			}
			return nullptr;
		case TOK_CHANGED:
			// Compare input value to old value, if different then call function 
			{
				BasicBlock *oldBlock = (*pin.writeAccessor)->getParent();
				Function* parentFunction = oldBlock->getParent();
				context.gContext.scopingStack.push(parentFunction->getSubprogram());

				context.pushBlock(oldBlock,debugLocation);
				Value* prior = CIdentifier::GetAliasedData(context,pin.priorValue,pin);
				Value* writeInput = CIdentifier::GetAliasedData(context,pin.writeInput,pin);
				Value* answer=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,prior,writeInput, "", context.currentBlock());
				
				BasicBlock *ifcall = BasicBlock::Create(TheContext,"ifcall",parentFunction);
				BasicBlock *nocall = BasicBlock::Create(TheContext,"nocall",parentFunction);

				BranchInst::Create(nocall,ifcall,answer,context.currentBlock());
				context.popBlock(debugLocation);
				
				CallInst* fcall = CallInst::Create(function,"",ifcall);
				if (context.gContext.opts.generateDebug)
				{
					fcall->setDebugLoc(DebugLoc::get(debugLocation.first_line, debugLocation.first_column, context.gContext.scopingStack.top()));
				}
				BranchInst::Create(nocall,ifcall);

				// Remove return instruction (since we need to create a new basic block set
				(*pin.writeAccessor)->removeFromParent();

				*pin.writeAccessor=ReturnInst::Create(TheContext,nocall);

				context.gContext.scopingStack.pop();
			}
			return nullptr;
		case TOK_TRANSITION:
			// Compare input value to param2 and old value to param1 , if same call function 
			{
				BasicBlock *oldBlock = (*pin.writeAccessor)->getParent();
				Function* parentFunction = oldBlock->getParent();
				context.gContext.scopingStack.push(parentFunction->getSubprogram());

				context.pushBlock(oldBlock,debugLocation);
				Value* prior = CIdentifier::GetAliasedData(context,pin.priorValue,pin);
				Value* writeInput = CIdentifier::GetAliasedData(context,pin.writeInput,pin);

				Value* checkParam2=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,ConstantInt::get(TheContext,param2.integer.zextOrTrunc(pin.trueSize.getLimitedValue())),writeInput, "", context.currentBlock());
				Value* checkParam1=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,ConstantInt::get(TheContext,param1.integer.zextOrTrunc(pin.trueSize.getLimitedValue())),prior, "", context.currentBlock());
		
				Value *answer=BinaryOperator::Create(Instruction::And,checkParam1,checkParam2,"",context.currentBlock());
				
				BasicBlock *ifcall = BasicBlock::Create(TheContext,"ifcall",parentFunction);
				BasicBlock *nocall = BasicBlock::Create(TheContext,"nocall",parentFunction);

				BranchInst::Create(ifcall,nocall,answer,context.currentBlock());
				context.popBlock(debugLocation);
				
				CallInst* fcall = CallInst::Create(function,"",ifcall);
				if (context.gContext.opts.generateDebug)
				{
					fcall->setDebugLoc(DebugLoc::get(debugLocation.first_line, debugLocation.first_column, context.gContext.scopingStack.top()));
				}
				BranchInst::Create(nocall,ifcall);

				// Remove return instruction (since we need to create a new basic block set
				(*pin.writeAccessor)->removeFromParent();

				*pin.writeAccessor=ReturnInst::Create(TheContext,nocall);

				context.gContext.scopingStack.pop();

			}
			return nullptr;

		default:
			assert(0 && "Unhandled trigger type");
			break;
	}

	return nullptr;
}

void CFunctionDecl::prePass(CodeGenContext& context)
{
	block.prePass(context);
}

Value* CFunctionDecl::codeGen(CodeGenContext& context)
{
	int a;
	BitVariable returnVal;
  	std::vector<Type*> FuncTy_8_args;

	for (a=0;a<params.size();a++)
	{
		unsigned size = params[a]->size.integer.getLimitedValue();
		FuncTy_8_args.push_back(IntegerType::get(TheContext, size));
	}
	FunctionType* FuncTy_8;

	if (returns.empty())
	{
		FuncTy_8 = FunctionType::get(Type::getVoidTy(TheContext),FuncTy_8_args,false);
	}
	else
	{
		unsigned size = returns[0]->size.integer.getLimitedValue();
		FuncTy_8 = FunctionType::get(IntegerType::get(TheContext, size),FuncTy_8_args,false);
	}

	Function* func = nullptr;
	if (internal || !context.isRoot)
	{
		func=Function::Create(FuncTy_8,GlobalValue::PrivateLinkage,context.symbolPrepend+name.name,context.module);
	}
	else
	{
		func=Function::Create(FuncTy_8,GlobalValue::ExternalLinkage,context.symbolPrepend+name.name,context.module);
		func->setCallingConv(CallingConv::C);
	}
	func->setDoesNotThrow();
	
	context.StartFunctionDebugInfo(func, functionLoc);

	context.m_externFunctions[name.name] = func;

	BasicBlock *bblock = BasicBlock::Create(TheContext, "entry", func, 0);
	
	context.pushBlock(bblock,block.blockStartLoc);

	if (!returns.empty())
	{
		returnVal.size = returns[0]->size.integer;
		returnVal.trueSize = returns[0]->size.integer;
		returnVal.cnst = APInt(returns[0]->size.integer.getLimitedValue(),0);
		returnVal.mask = ~returnVal.cnst;
		returnVal.shft = APInt(returns[0]->size.integer.getLimitedValue(),0);
		returnVal.aliased = false;
		returnVal.mappingRef = false;
		returnVal.pinType=0;
		returnVal.writeAccessor=nullptr;
		returnVal.writeInput=nullptr;
		returnVal.priorValue=nullptr;
		returnVal.impedance=nullptr;
		returnVal.fromExternal=false;

		AllocaInst *alloc = new AllocaInst(Type::getIntNTy(TheContext,returns[0]->size.integer.getLimitedValue()),0, returns[0]->id.name.c_str(), context.currentBlock());
		returnVal.value = alloc;
		context.locals()[returns[0]->id.name] = returnVal;
	}
	
	Function::arg_iterator args = func->arg_begin();
	a=0;
	while (args!=func->arg_end())
	{
		BitVariable temp;
		temp.size = params[a]->size.integer;
		temp.trueSize = params[a]->size.integer;
		temp.cnst = APInt(params[a]->size.integer.getLimitedValue(),0);
		temp.mask = ~temp.cnst;
		temp.shft = APInt(params[a]->size.integer.getLimitedValue(),0);
		temp.aliased = false;
		temp.mappingRef = false;
		temp.pinType=0;
		temp.writeAccessor=nullptr;
		temp.writeInput=nullptr;
		temp.priorValue=nullptr;
		temp.impedance=nullptr;
		temp.fromExternal=false;

		temp.value=&*args;
		temp.value->setName(params[a]->id.name);
		context.locals()[params[a]->id.name]=temp;
		a++;
		args++;
	}

	block.codeGen(context);

	if (returns.empty())
	{
		ReturnInst::Create(TheContext, context.currentBlock());			/* block may well have changed by time we reach here */
	}
	else
	{
		ReturnInst::Create(TheContext,new LoadInst(returnVal.value,"",context.currentBlock()),context.currentBlock());
	}

	context.popBlock(block.blockEndLoc);

	context.EndFunctionDebugInfo();

	return func;
}

extern int yyparse();
extern CBlock* g_ProgramBlock;
extern int resetFileInput(const char* filename);

#include <stdio.h>

void CInstance::prePass(CodeGenContext& context)
{
	// On the prepass, we need to generate the code for this module
	std::string includeName = filename.quoted.substr(1,filename.quoted.length()-2);

	if (resetFileInput(includeName.c_str())!=0)
	{
		PrintErrorFromLocation(filename.quotedLoc, "Unable to instance module %s", includeName.c_str());
		context.errorFlagged=true;
		return;
	}
	yyparse();
	if (g_ProgramBlock==0)
	{
		PrintErrorFromLocation(filename.quotedLoc, "Unable to parse module %s", includeName.c_str());
		context.errorFlagged=true;
		return;
	}
	
	CodeGenContext* includefile;

	includefile = new CodeGenContext(context.gContext,&context);
	includefile->moduleName=ident.name+".";

	if (context.gContext.opts.generateDebug)
	{
		context.gContext.scopingStack.push(CreateNewDbgFile(includeName.c_str(), includefile->dbgBuilder));
	}

	includefile->generateCode(*g_ProgramBlock);

	if (context.gContext.opts.generateDebug)
	{
		context.gContext.scopingStack.pop();
	}
	if (includefile->errorFlagged)
	{
		PrintErrorFromLocation(filename.quotedLoc, "Unable to parse module %s", includeName.c_str());
		context.errorFlagged = true;
		return;
	}
	context.m_includes[ident.name+"."]=includefile;
}

Value* CInstance::codeGen(CodeGenContext& context)
{
	// At present doesn't generate any code - maybe never will
	return nullptr;
}

Value* CExchange::codeGen(CodeGenContext& context)
{
	BitVariable lhsVar,rhsVar;

	if (!context.LookupBitVariable(lhsVar,lhs.module,lhs.name,lhs.modLoc,lhs.nameLoc))
	{
		return nullptr;
	}
	if (!context.LookupBitVariable(rhsVar,rhs.module,rhs.name,rhs.modLoc,rhs.nameLoc))
	{
		return nullptr;
	}

	if (lhsVar.value->getType()->isPointerTy() && rhsVar.value->getType()->isPointerTy())
	{
		// Both sides must be identifiers

		Value* left = lhs.codeGen(context);
		Value* right= rhs.codeGen(context);
	
		if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
		{
			const IntegerType* leftType = cast<IntegerType>(left->getType());
			const IntegerType* rightType = cast<IntegerType>(right->getType());

			if (leftType->getBitWidth() != rightType->getBitWidth())
			{
				PrintErrorFromLocation(operatorLoc,"Both operands to exchange must be same size");
				context.errorFlagged=true;
				return nullptr;
			}
		
			CAssignment::generateAssignment(lhsVar,lhs,right,context);
			CAssignment::generateAssignment(rhsVar,rhs,left,context);
		}
		
		return nullptr;
	}
	
	PrintErrorFromLocation(operatorLoc, "Illegal operands to exchange");
	context.errorFlagged=true;
	return nullptr;
}

