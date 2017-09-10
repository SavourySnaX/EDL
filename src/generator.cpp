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
			if (context->gContext.connectFunctions.find(&F)!=context->gContext.connectFunctions.end())
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
		if (gContext.opts.symbolModifier)
		{
			symbolPrepend=gContext.opts.symbolModifier;
		}

		// Testing - setup a compile unit for our root module
		if (gContext.opts.generateDebug)
		{
			gContext.scopingStack.push(CreateNewDbgFile(gContext.opts.inputFile, dbgBuilder));

			compileUnit = dbgBuilder->createCompileUnit(/*0x9999*/dwarf::DW_LANG_C99, gContext.scopingStack.top()->getFile(), "edlVxx", gContext.opts.optimisationLevel, ""/*command line flags*/, 0);

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
