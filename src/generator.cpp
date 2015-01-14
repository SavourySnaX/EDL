#include "ast.h"
#include "generator.h"
#include "parser.hpp"


#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"

using namespace llvm;

CInteger::CInteger(std::string& value )
{
	unsigned char radix;
	const char* data;
	unsigned numBits;
	if (value[0]=='%')
	{
		numBits = value.length()-1;
		data = &value.c_str()[1];
		radix=2;
	}
	else
	{
		if (value[0]=='$')
		{
			numBits = 4*(value.length()-1);
			data = &value.c_str()[1];
			radix=16;
		}
		else
		{
			numBits = 4*(value.length());	/* over allocation for now */
			data = &value.c_str()[0];
			radix=10;
		}
	}

	integer = llvm::APInt(numBits,data,radix);
	if (radix==10)
	{
		if (integer.getActiveBits())						// this shrinks the value to the correct number of bits - fixes over allocation for decimal numbers
		{
			if (integer.getActiveBits()!=numBits)
			{
				integer = integer.trunc(integer.getActiveBits());		// only performed on decimal numbers (otherwise we loose important leading zeros)
			}
		}
		else
		{
			integer = integer.trunc(1);
		}
	}
}


#define DEBUG_STATE_SQUASH	0

namespace 
{
	struct StateReferenceSquasher : public FunctionPass 
	{
		CodeGenContext *ourContext;

		static char ID;
		StateReferenceSquasher() : ourContext(NULL),FunctionPass(ID) {}
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
				Value* foundReference=NULL;
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
		InOutReferenceSquasher() : ourContext(NULL),FunctionPass(ID) {}
		InOutReferenceSquasher(CodeGenContext* context) : ourContext(context),FunctionPass(ID) {}

		bool DoWork(Function &F,CodeGenContext* context)
		{
			BasicBlock& hoistHere = F.getEntryBlock();
			TerminatorInst* hoistBefore = hoistHere.getTerminator();

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
					Value* foundReference=NULL;
				
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

CodeGenContext::CodeGenContext(CodeGenContext* parent) 
{ 
	if (!parent) 
	{ 
		std::string err;
		module = new Module("root",getGlobalContext()); 
		ee = EngineBuilder(module).setErrorStr(&err).create();
		isRoot=true; 
	}
	else
	{
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

void CodeGenContext::GenerateDisassmTables()
{
	std::map<std::string, std::map<APInt,std::string,myAPIntCompare> >::iterator tableIter;

	for (tableIter=disassemblyTable.begin();tableIter!=disassemblyTable.end();++tableIter)
	{
		APInt tableSize=tableIter->second.rbegin()->first;
		APInt tableSize32=tableSize.zextOrTrunc(32);
		// Create a global variable to indicate the max size of the table

		GlobalVariable* gvar_int32_DIS_max = new GlobalVariable(*module, IntegerType::get(getGlobalContext(), 32),true,GlobalValue::ExternalLinkage,NULL,symbolPrepend+"DIS_max_"+tableIter->first);
		ConstantInt* const_int32_1 = ConstantInt::get(getGlobalContext(), tableSize32+1);
		gvar_int32_DIS_max->setInitializer(const_int32_1);

		// Create a global array to hold the table
		PointerType* PointerTy_5 = PointerType::get(IntegerType::get(getGlobalContext(), 8), 0);
	       	ArrayType* ArrayTy_4 = ArrayType::get(PointerTy_5, tableSize.getLimitedValue()+1);
		ConstantPointerNull* const_ptr_13 = ConstantPointerNull::get(PointerTy_5);	
		GlobalVariable* gvar_array_table = new GlobalVariable(*module,ArrayTy_4,true,GlobalValue::ExternalLinkage,NULL, symbolPrepend+"DIS_"+tableIter->first);
		std::vector<Constant*> const_array_9_elems;

		std::map<APInt,std::string,myAPIntCompare>::iterator slot=tableIter->second.begin();
		APInt trackingSlot(tableSize.getBitWidth(),"0",16);

		while (slot!=tableIter->second.end())
		{
			if (CompareEquals(slot->first,trackingSlot))
			{
				ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(getGlobalContext(), 8), slot->second.length()-1);
				GlobalVariable* gvar_array__str = new GlobalVariable(*module, ArrayTy_0,true,GlobalValue::PrivateLinkage,0,symbolPrepend+".str"+trackingSlot.toString(16,false));
				gvar_array__str->setAlignment(1);
  
				Constant* const_array_9 = ConstantDataArray::getString(getGlobalContext(), slot->second.substr(1,slot->second.length()-2), true);
				std::vector<Constant*> const_ptr_12_indices;
				ConstantInt* const_int64_13 = ConstantInt::get(getGlobalContext(), APInt(64, StringRef("0"), 10));
				const_ptr_12_indices.push_back(const_int64_13);
				const_ptr_12_indices.push_back(const_int64_13);
				Constant* const_ptr_12 = ConstantExpr::getGetElementPtr(gvar_array__str, const_ptr_12_indices);

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
void CodeGenContext::generateCode(CBlock& root,CompilerOptions &options)
{
	errorFlagged = false;

	if (isRoot)
	{
		opts=options;
		if (opts.symbolModifier)
		{
			symbolPrepend=opts.symbolModifier;
		}

		PointerType* PointerTy_4 = PointerType::get(IntegerType::get(getGlobalContext(), 8), 0);

		std::vector<Type*>FuncTy_8_args;
		FuncTy_8_args.push_back(PointerTy_4);
		FunctionType* FuncTy_8 = FunctionType::get(/*Result=*/IntegerType::get(getGlobalContext(), 32),/*Params=*/FuncTy_8_args,/*isVarArg=*/false);

		debugTraceString = Function::Create(/*Type=*/FuncTy_8,/*Linkage=*/GlobalValue::ExternalLinkage,/*Name=*/"puts", module); // (external, no body)
		debugTraceString->setCallingConv(CallingConv::C);

		std::vector<Type*>FuncTy_6_args;
		FuncTy_6_args.push_back(IntegerType::get(getGlobalContext(), 32));
		FunctionType* FuncTy_6 = FunctionType::get(/*Result=*/IntegerType::get(getGlobalContext(), 32),/*Params=*/FuncTy_6_args,/*isVarArg=*/false);

		debugTraceChar = Function::Create(/*Type=*/FuncTy_6,/*Linkage=*/GlobalValue::ExternalLinkage,/*Name=*/"putchar", module); // (external, no body)
		debugTraceChar->setCallingConv(CallingConv::C);
		
		debugTraceMissing = Function::Create(FuncTy_6,GlobalValue::ExternalLinkage,symbolPrepend+"missing", module); // (external, no body)
		debugTraceMissing->setCallingConv(CallingConv::C);
	}

	root.prePass(*this);	/* Run a pre-pass on the code */
	root.codeGen(*this);	/* Generate complete code - starting at no block (global space) */

	// Finalise disassembly arrays
	if (opts.generateDisassembly)
	{
		GenerateDisassmTables();
	}
	
	if (errorFlagged)
	{
		std::cerr << "Compilation Failed" << std::endl;
		return;
	}

	if (isRoot)
	{
		/* Print the bytecode in a human-readable format 
		   to see if our program compiled properly
		   */
		PassManager pm;

		pm.add(createVerifierPass());
		
		if (opts.optimisationLevel>0)
		{
			// Add an appropriate TargetData instance for this module...
			pm.add(new DataLayout(*ee->getDataLayout()));

			for (int a=0;a<2;a++)		// We add things twice, as the statereferencesquasher will make more improvements once inlining has happened
			{
				if (opts.optimisationLevel>1)
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
				pm.add(createScalarReplAggregatesPass()); // Break up allocas

				// Run a few AA driven optimizations here and now, to cleanup the code.
				pm.add(createFunctionAttrsPass());        // Add nocapture
				pm.add(createGlobalsModRefPass());        // IP alias analysis
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
				pm.add(createFunctionAttrsPass());        // Deduce function attrs

				//  if (!DisableInline)
				pm.add(createFunctionInliningPass());   // Inline small functions
				pm.add(createArgumentPromotionPass());    // Scalarize uninlined fn args

				pm.add(createInstructionCombiningPass()); // Cleanup for scalarrepl.
				pm.add(createJumpThreadingPass());        // Thread jumps.
				pm.add(createCFGSimplificationPass());    // Merge & remove BBs
				pm.add(createScalarReplAggregatesPass()); // Break up aggregate allocas
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
		pm.add(createPrintModulePass(&outs()));

		pm.run(*module);
	}
}

/* Executes the AST by running the main function */
GenericValue CodeGenContext::runCode() 
{
	GenericValue v;

	return v;
}

/* -- Code Generation -- */

Value* CInteger::codeGen(CodeGenContext& context)
{
	return ConstantInt::get(getGlobalContext(), integer);
}

void CString::prePass(CodeGenContext& context)
{
}

Value* CString::codeGen(CodeGenContext& context)
{
	ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(getGlobalContext(), 8), quoted.length()-1);
 
       	GlobalVariable* gvar_array__str = new GlobalVariable(/*Module=*/*context.module,  /*Type=*/ArrayTy_0,  /*isConstant=*/true,  /*Linkage=*/GlobalValue::PrivateLinkage,  /*Initializer=*/0,  /*Name=*/context.symbolPrepend+".str");
	gvar_array__str->setAlignment(1);
  
	// Constant Definitions
	Constant* const_array_9 = ConstantDataArray::getString(getGlobalContext(), quoted.substr(1,quoted.length()-2), true);
	std::vector<Constant*> const_ptr_12_indices;
	ConstantInt* const_int64_13 = ConstantInt::get(getGlobalContext(), APInt(64, StringRef("0"), 10));
	const_ptr_12_indices.push_back(const_int64_13);
	const_ptr_12_indices.push_back(const_int64_13);
	Constant* const_ptr_12 = ConstantExpr::getGetElementPtr(gvar_array__str, const_ptr_12_indices);

	// Global Variable Definitions
	gvar_array__str->setInitializer(const_array_9);

	return const_ptr_12;
}

Value* CIdentifier::trueSize(Value* in,CodeGenContext& context,BitVariable& var)
{
	if (var.mappingRef)
	{
		std::cerr << "Cannot perform operation on a mapping Ref!" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	Type* ty = Type::getIntNTy(getGlobalContext(),var.trueSize.getLimitedValue());
	Instruction::CastOps op = CastInst::getCastOpcode(in,false,ty,false);

	Instruction* truncExt = CastInst::Create(op,in,ty,"cast",context.currentBlock());

	return truncExt;
}

Value* CIdentifier::GetAliasedData(CodeGenContext& context,Value* in,BitVariable& var)
{
	if (var.aliased == true)
	{
		// We are loading from a partial value - we need to load, mask off correct result and shift result down to correct range
		ConstantInt* const_intMask = ConstantInt::get(getGlobalContext(), var.mask);	
		BinaryOperator* andInst = BinaryOperator::Create(Instruction::And, in, const_intMask , "Masking", context.currentBlock());
		ConstantInt* const_intShift = ConstantInt::get(getGlobalContext(), var.shft);
		BinaryOperator* shiftInst = BinaryOperator::Create(Instruction::LShr,andInst ,const_intShift, "Shifting", context.currentBlock());
		return trueSize(shiftInst,context,var);
	}

	return in;
}

bool CodeGenContext::LookupBitVariable(BitVariable& outVar,const std::string& module, const std::string& name)
{
	if ((currentBlock()!=NULL) && (locals().find(name) != locals().end()))
	{
		outVar=locals()[name];
	}
	else
	{
		if (globals().find(name) == globals().end())
		{
			if (m_includes.find(module)!=m_includes.end())
			{
				if (m_includes[module]->LookupBitVariable(outVar,"",name))
				{
					if (outVar.pinType!=0)		// TODO: Globals Vars!
					{
						outVar.fromExternal=true;
						return true;
					}
				}
			}
			std::cerr << "undeclared variable " << name << endl;
			errorFlagged=true;
			return false;
		}
		else
		{
			outVar=globals()[name];
		}
	}

	outVar.fromExternal=false;
	return true;
}

Function* CodeGenContext::LookupFunctionInExternalModule(const std::string& moduleName, const std::string& name)
{
	Function* function = m_includes[moduleName]->module->getFunction(name);

	return function;
}

Value* CIdentifier::codeGen(CodeGenContext& context)
{
	BitVariable var;

	if (!context.LookupBitVariable(var,module,name))
	{
		return NULL;
	}

	if (var.mappingRef)
	{
		return var.mapping->codeGen(context);
	}
	
	if (var.fromExternal)
	{
		if (var.pinType!=TOK_OUT && var.pinType!=TOK_BIDIRECTIONAL)
		{
			std::cerr << "Pin marked as non readable in module!" << name << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		else
		{
			// At this point, we need to get PinGet method and call it.
			
			Function* function=context.LookupFunctionInExternalModule(module,context.symbolPrepend+"PinGet"+name);

			std::vector<Value*> args;
			return CallInst::Create(function,args,"pinValue",context.currentBlock());
		}

	}

	if (var.value->getType()->isPointerTy())
	{
		Value* final = NULL;
		if (IsArray())
		{
			Value* exprResult=GetExpression()->codeGen(context);

			Type* ty = Type::getIntNTy(getGlobalContext(),var.arraySize.getLimitedValue());
			Type* ty64 = Type::getIntNTy(getGlobalContext(),64);
			Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,ty,false);
			Instruction* truncExt0 = CastInst::Create(op,exprResult,ty,"cast",context.currentBlock());		// Cast index to 64 bit type
			Instruction::CastOps op1 = CastInst::getCastOpcode(exprResult,false,ty64,false);
			Instruction* truncExt = CastInst::Create(op1,truncExt0,ty64,"cast",context.currentBlock());		// Cast index to 64 bit type

 			std::vector<Value*> indices;
 			ConstantInt* index0 = ConstantInt::get(getGlobalContext(), APInt(var.size.getLimitedValue(), StringRef("0"), 10));
			indices.push_back(index0);
 			indices.push_back(truncExt);
			Instruction* elementPtr = GetElementPtrInst::Create(var.value,indices,"array index",context.currentBlock());

			final = new LoadInst(elementPtr, "", false, context.currentBlock());
		}
		else
		{
			final = new LoadInst(var.value, "", false, context.currentBlock());
		}
		return GetAliasedData(context,final,var);
	}

	return var.value;
}

CIdentifier CAliasDeclaration::empty("");

Value* CDebugTraceString::codeGen(CodeGenContext& context)
{
	for (unsigned a=1;a<string.quoted.length()-1;a++)
	{
		std::vector<Value*> args;
		
		args.push_back(ConstantInt::get(getGlobalContext(),APInt(32,string.quoted[a])));
		CallInst *call = CallInst::Create(context.debugTraceChar,args,"DEBUGTRACE",context.currentBlock());
	}
}

Value* CDebugTraceInteger::codeGen(CodeGenContext& context)
{
	static char tbl[37]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	CallInst* fcall;
	unsigned bitWidth = integer.integer.getBitWidth();
	// compute max divisor for associated base and bit width
	int cnt=1;

	APInt baseDivisor(bitWidth*2,currentBase);
	APInt oldBaseDivisor = baseDivisor;
	while (baseDivisor.getActiveBits()-1 < bitWidth)
	{
		oldBaseDivisor = baseDivisor;
		baseDivisor*=APInt(bitWidth*2,currentBase);

		cnt++;
	}

	baseDivisor=oldBaseDivisor.trunc(bitWidth);
	if (baseDivisor.getLimitedValue()==0)
	{
		baseDivisor = APInt(bitWidth,1);
	}

	for (unsigned a=0;a<cnt;a++)
	{
		std::vector<Value*> args;

		APInt tmp = integer.integer.udiv(baseDivisor);

		args.push_back(ConstantInt::get(getGlobalContext(), APInt(32, tbl[tmp.getLimitedValue()])));
		CallInst *call = CallInst::Create(context.debugTraceChar,args,"DEBUGTRACE",context.currentBlock());

		integer.integer-=tmp*baseDivisor;
		if (cnt!=1)
		{
			baseDivisor=baseDivisor.udiv(APInt(bitWidth,currentBase));
		}
	}
	return fcall;
}

// The below provides the output for a single expr, will be called multiple times for arrays (at least for now)
Value* CDebugTraceIdentifier::generate(CodeGenContext& context,Value *loadedValue)
{
	const IntegerType* valueType = cast<IntegerType>(loadedValue->getType());
	unsigned bitWidth = valueType->getBitWidth();

	// compute max divisor for associated base and bit width
	int cnt=1;

	APInt baseDivisor(bitWidth*2,currentBase);
	APInt oldBaseDivisor = baseDivisor;
	while (baseDivisor.getActiveBits()-1 < bitWidth)
	{
		oldBaseDivisor = baseDivisor;
		baseDivisor*=APInt(bitWidth*2,currentBase);

		cnt++;
	}

	baseDivisor=oldBaseDivisor.trunc(bitWidth);
	if (baseDivisor.getLimitedValue()==0)
	{
		baseDivisor = APInt(bitWidth,1);
	}

	for (unsigned a=0;a<cnt;a++)
	{
		std::vector<Value*> args;
		
		ConstantInt* const_intDiv = ConstantInt::get(getGlobalContext(), baseDivisor);
		BinaryOperator* shiftInst = BinaryOperator::Create(Instruction::UDiv,loadedValue,const_intDiv, "Dividing", context.currentBlock());
		
		Type* ty = Type::getIntNTy(getGlobalContext(),32);
		Instruction::CastOps op = CastInst::getCastOpcode(shiftInst,false,ty,false);
		Instruction* readyToAdd = CastInst::Create(op,shiftInst,ty,"cast",context.currentBlock());
		
		CmpInst* check=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_ULE,readyToAdd,ConstantInt::get(getGlobalContext(),APInt(32,9)),"rangeCheck",context.currentBlock());

		BinaryOperator* lowAdd = BinaryOperator::Create(Instruction::Add,readyToAdd,ConstantInt::get(getGlobalContext(),APInt(32,'0')),"lowAdd",context.currentBlock());
		BinaryOperator* hiAdd = BinaryOperator::Create(Instruction::Add,readyToAdd,ConstantInt::get(getGlobalContext(),APInt(32,'A'-10)),"hiAdd",context.currentBlock());

		SelectInst *select = SelectInst::Create(check,lowAdd,hiAdd,"getRightChar",context.currentBlock());

		args.push_back(select);
		CallInst *call = CallInst::Create(context.debugTraceChar,args,"DEBUGTRACE",context.currentBlock());
		
		BinaryOperator* mulUp = BinaryOperator::Create(Instruction::Mul,shiftInst,const_intDiv,"mulUp",context.currentBlock());
		loadedValue = BinaryOperator::Create(Instruction::Sub,loadedValue,mulUp,"fixup",context.currentBlock());
		if (cnt!=1)
		{
			baseDivisor=baseDivisor.udiv(APInt(bitWidth,currentBase));
		}
	}
	return NULL;
}

Value* CDebugTraceIdentifier::codeGen(CodeGenContext& context)
{
	int a;
	BitVariable var;

	if (!context.LookupBitVariable(var,ident.module,ident.name))
	{
		return NULL;
	}

	if (var.arraySize.getLimitedValue()==0 || ident.IsArray())
	{
		std::string tmp = "\"";
		tmp+=ident.name;
		tmp+="(\"";
		CString strng(tmp);
		CDebugTraceString identName(strng);

		identName.codeGen(context);

		Value* loadedValue = ident.codeGen(context);
		
		Value* ret=generate(context,loadedValue);

		std::vector<Value*> args;
		args.push_back(ConstantInt::get(getGlobalContext(), APInt(32, ')')));
		CallInst::Create(context.debugTraceChar,args,"DEBUGTRACE",context.currentBlock());

		return ret;
	}

	// presume they want the entire contents of array dumping

	{
		std::string tmp = "\"";
		tmp+=ident.name;
		tmp+="(\"";
		CString strng(tmp);
		CDebugTraceString identName(strng);
		
		identName.codeGen(context);

		APInt power2(var.arraySize.getLimitedValue()+1,1);
		power2<<=var.arraySize.getLimitedValue();
		for (a=0;a<power2.getLimitedValue();a++)
		{
			std::string tmp;
			tmp+=Twine(a).str();
			CInteger index(tmp);
			CIdentifierArray idArrayTemp(index,ident.name);

			generate(context,idArrayTemp.codeGen(context));
			if (a!=power2.getLimitedValue()-1)
			{
				std::vector<Value*> args;
				args.push_back(ConstantInt::get(getGlobalContext(), APInt(32, ',')));
				CallInst::Create(context.debugTraceChar,args,"DEBUGTRACE",context.currentBlock());
			}
		}
		
		std::vector<Value*> args;
		args.push_back(ConstantInt::get(getGlobalContext(), APInt(32, ')')));
		CallInst::Create(context.debugTraceChar,args,"DEBUGTRACE",context.currentBlock());
	}

	return NULL;
}


Value* CDebugLine::codeGen(CodeGenContext& context)		// Refactored away onto its own block - TODO factor away to a function - need to take care of passing identifiers forward though
{
	int currentBase=2;

	BasicBlock *trac = BasicBlock::Create(getGlobalContext(),"debug_trace_helper",context.currentBlock()->getParent());
	BasicBlock *cont = BasicBlock::Create(getGlobalContext(),"continue",context.currentBlock()->getParent());
	
	BranchInst::Create(trac,context.currentBlock());

	context.pushBlock(trac);

	for (unsigned a=0;a<debug.size();a++)
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

	std::vector<Value*> args;
	args.push_back(ConstantInt::get(getGlobalContext(), APInt(32, '\n')));
	CallInst* fcall = CallInst::Create(context.debugTraceChar,args,"DEBUGTRACE",context.currentBlock());
	
	BranchInst::Create(cont,context.currentBlock());

	context.popBlock();

	context.setBlock(cont);

	return NULL;
}

bool CBinaryOperator::IsCarryExpression()
{
	bool allCarry=true;

	if (!lhs.IsLeaf())
		allCarry&=lhs.IsCarryExpression();
	if (!rhs.IsLeaf())
		allCarry&=rhs.IsCarryExpression();

	return (op==TOK_ADD || op==TOK_DADD || op==TOK_DSUB || op==TOK_SUB) && allCarry;
}

void CBinaryOperator::prePass(CodeGenContext& context)
{
	lhs.prePass(context);
	rhs.prePass(context);
}

Value* CBinaryOperator::codeGen(CodeGenContext& context)
{
	Value* left;
	Value* right;

	left = lhs.codeGen(context);
	right = rhs.codeGen(context);

	return codeGen(context,left,right);
}

Value* CBinaryOperator::codeGen(CodeGenContext& context,Value* left,Value* right)
{
	if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
	{
		IntegerType* leftType = cast<IntegerType>(left->getType());
		IntegerType* rightType = cast<IntegerType>(right->getType());

		bool signExtend;
		switch (op)
		{
		case TOK_DADD:
		case TOK_DSUB:
			signExtend=true;
			break;
		default:
			signExtend=false;
			break;
		}

		if (leftType->getBitWidth() < rightType->getBitWidth())
		{
			Instruction::CastOps op = CastInst::getCastOpcode(left,signExtend,rightType,false);

			left = CastInst::Create(op,left,rightType,"cast",context.currentBlock());
		}
		if (leftType->getBitWidth() > rightType->getBitWidth())
		{
			Instruction::CastOps op = CastInst::getCastOpcode(right,signExtend,leftType,false);

			right = CastInst::Create(op,right,leftType,"cast",context.currentBlock());
		}

		switch (op) 
		{
		case TOK_ADD:
		case TOK_DADD:
		case TOK_SUB:
		case TOK_DSUB:
			{
				Value* result;
			       	if (op==TOK_ADD || op==TOK_DADD)
				{
					result=BinaryOperator::Create(Instruction::Add,left,right,"",context.currentBlock());
				}
				else
				{
					result=BinaryOperator::Create(Instruction::Sub,left,right,"",context.currentBlock());
				}
				
				for (int a=0;a<context.curAffectors.size();a++)
				{
					switch (context.curAffectors[a]->type)
					{
						case TOK_NOOVERFLOW:
						case TOK_OVERFLOW:
							if (context.curAffectors[a]->opType==TOK_NOOVERFLOW || context.curAffectors[a]->opType==TOK_OVERFLOW)
							{
								context.curAffectors[a]->opType=op;
							}
							else
							{
								switch(context.curAffectors[a]->opType)
								{
									case TOK_ADD:
									case TOK_DADD:
										if (op!=TOK_ADD && op!=TOK_DADD)
										{
											std::cerr<<"Can't Compute OVERFLOW for differing mathematical operations"<<std::endl;
											context.errorFlagged=true;
											return NULL;
										}
										break;
									case TOK_SUB:
									case TOK_DSUB:
										if (op!=TOK_SUB && op!=TOK_DSUB)
										{
											std::cerr<<"Can't Compute OVERFLOW for differing mathematical operations"<<std::endl;
											context.errorFlagged=true;
											return NULL;
										}
										break;
								}
							}
							break;
						case TOK_NOCARRY:
						case TOK_CARRY:
							context.curAffectors[a]->codeGenCarry(context,result,left,right,op);
							break;
					}
				}

				return result;
			}
		case TOK_MUL:
			return BinaryOperator::Create(Instruction::Mul,left,right,"",context.currentBlock());
		case TOK_DIV:
			return BinaryOperator::Create(Instruction::UDiv,left,right,"",context.currentBlock());
		case TOK_MOD:
			return BinaryOperator::Create(Instruction::URem,left,right,"",context.currentBlock());
		case TOK_SDIV:
			return BinaryOperator::Create(Instruction::SDiv,left,right,"",context.currentBlock());
		case TOK_SMOD:
			return BinaryOperator::Create(Instruction::SRem,left,right,"",context.currentBlock());
		case TOK_CMPEQ:
			return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,left, right, "", context.currentBlock());
		case TOK_CMPNEQ:
			return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_NE,left, right, "", context.currentBlock());
		case TOK_CMPLESS:
			return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_ULT,left, right, "", context.currentBlock());
		case TOK_CMPLESSEQ:
			return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_ULE,left, right, "", context.currentBlock());
		case TOK_CMPGREATER:
			return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_UGT,left, right, "", context.currentBlock());
		case TOK_CMPGREATEREQ:
			return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_UGE,left, right, "", context.currentBlock());
		case TOK_BAR:
			return BinaryOperator::Create(Instruction::Or,left,right,"",context.currentBlock());
		case TOK_AMP:
			return BinaryOperator::Create(Instruction::And,left,right,"",context.currentBlock());
		case TOK_HAT:
			return BinaryOperator::Create(Instruction::Xor,left,right,"",context.currentBlock());
		case TOK_TILDE:
			return BinaryOperator::Create(Instruction::Xor,left,ConstantInt::get(getGlobalContext(),~APInt(leftType->getBitWidth(),0)),"",context.currentBlock());
		}
	}

	std::cerr << "Illegal types in expression" << std::endl;
	
	context.errorFlagged=true;

	return NULL;
}

std::string stringZero("0");

CInteger CCastOperator::begZero(stringZero);

void CCastOperator::prePass(CodeGenContext& context)
{
	lhs.prePass(context);
}

Value* CCastOperator::codeGen(CodeGenContext& context)
{
	// Compute a mask between specified bits, shift result down by start bits
	
    if (lhs.IsAssignmentExpression())
	{
		CAssignment* assign=(CAssignment*)&lhs;
		return assign->codeGen(context,this);
	}
	else
	{
		Value* left = lhs.codeGen(context);
		if (left->getType()->isIntegerTy())
		{
			const IntegerType* leftType = cast<IntegerType>(left->getType());

			APInt mask(leftType->getBitWidth(),"0",10);
			APInt start=beg.integer;
			APInt loop=beg.integer.zextOrTrunc(leftType->getBitWidth());
			APInt endloop = end.integer.zextOrTrunc(leftType->getBitWidth());
			start=start.zextOrTrunc(leftType->getBitWidth());

			while (1==1)
			{
				mask.setBit(loop.getLimitedValue());
				if (loop.uge(endloop))
					break;
				loop++;
			}

			Value *masked=BinaryOperator::Create(Instruction::And,left,ConstantInt::get(getGlobalContext(),mask),"castMask",context.currentBlock());
			Value *shifted=BinaryOperator::Create(Instruction::LShr,masked,ConstantInt::get(getGlobalContext(),start),"castShift",context.currentBlock());

			// Final step cast it to correct size - not actually required, will be handled by expr lowering/raising anyway
			return shifted;
		}
	}
	std::cerr << "Illegal type in cast" << std::endl;
	
	context.errorFlagged=true;

	return NULL;
}

void CRotationOperator::prePass(CodeGenContext& context)
{
	value.prePass(context);
	bitsIn.prePass(context);
	rotAmount.prePass(context);
}

Value* CRotationOperator::codeGen(CodeGenContext& context)
{
	Value* toShift = value.codeGen(context);
	Value* shiftIn = bitsIn.codeGen(context);
	Value* rotBy = rotAmount.codeGen(context);
	BitVariable var;
	if (!context.LookupBitVariable(var,bitsOut.module,bitsOut.name))
	{
		return NULL;
	}
	
	if ((!toShift->getType()->isIntegerTy())||(!rotBy->getType()->isIntegerTy()))
	{
		std::cerr << "Unsupported operation" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	if (var.mappingRef)
	{
		std::cerr << "Cannot perform operation on a mappingRef" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	IntegerType* toShiftType = cast<IntegerType>(toShift->getType());

	if (direction == TOK_ROL)
	{
		Instruction::CastOps oprotby = CastInst::getCastOpcode(rotBy,false,toShiftType,false);
		Value *rotByCast = CastInst::Create(oprotby,rotBy,toShiftType,"cast",context.currentBlock());

		Value *amountToShift = BinaryOperator::Create(Instruction::Sub,ConstantInt::get(getGlobalContext(),APInt(toShiftType->getBitWidth(),toShiftType->getBitWidth())),rotByCast,"shiftAmount",context.currentBlock());

		Value *shiftedDown = BinaryOperator::Create(Instruction::LShr,toShift,amountToShift,"carryOutShift",context.currentBlock());

		CAssignment::generateAssignment(var,bitsOut,shiftedDown,context);

		Value *shifted=BinaryOperator::Create(Instruction::Shl,toShift,rotByCast,"rolShift",context.currentBlock());

		Instruction::CastOps op = CastInst::getCastOpcode(shiftIn,false,toShiftType,false);

		Value *upCast = CastInst::Create(op,shiftIn,toShiftType,"cast",context.currentBlock());

		Value *rotResult = BinaryOperator::Create(Instruction::Or,upCast,shifted,"rolOr",context.currentBlock());

		return rotResult;
	}
	else
	{
		Instruction::CastOps oprotby = CastInst::getCastOpcode(rotBy,false,toShiftType,false);
		Value *rotByCast = CastInst::Create(oprotby,rotBy,toShiftType,"cast",context.currentBlock());

		Value *amountToShift = BinaryOperator::Create(Instruction::Sub,ConstantInt::get(getGlobalContext(),APInt(toShiftType->getBitWidth(),toShiftType->getBitWidth())),rotByCast,"shiftAmount",context.currentBlock());

		APInt downMaskc(toShiftType->getBitWidth(),0);
		downMaskc=~downMaskc;
		Value *downMask = BinaryOperator::Create(Instruction::LShr,ConstantInt::get(getGlobalContext(),downMaskc),amountToShift,"downmask",context.currentBlock());

		Value *maskedDown = BinaryOperator::Create(Instruction::And,toShift,downMask,"carryOutMask",context.currentBlock());

		CAssignment::generateAssignment(var,bitsOut,maskedDown,context);

		Value *shifted=BinaryOperator::Create(Instruction::LShr,toShift,rotByCast,"rorShift",context.currentBlock());
			
		Instruction::CastOps op = CastInst::getCastOpcode(shiftIn,false,toShiftType,false);

		Value *upCast = CastInst::Create(op,shiftIn,toShiftType,"cast",context.currentBlock());

		Value *shiftedUp = BinaryOperator::Create(Instruction::Shl,upCast,amountToShift,"rorOrShift",context.currentBlock());
		
		Value *rotResult = BinaryOperator::Create(Instruction::Or,shiftedUp,shifted,"rorOr",context.currentBlock());

		return rotResult;
	}

	return value.codeGen(context);
}

Value* CAssignment::generateImpedanceAssignment(BitVariable& to,llvm::Value* assignTo,CodeGenContext& context)
{
	ConstantInt* impedance = ConstantInt::get(getGlobalContext(), ~APInt(to.size.getLimitedValue(), StringRef("0"), 10));

	return new StoreInst(impedance, assignTo, false, context.currentBlock());
}

Value* CAssignment::generateAssignment(BitVariable& to,const CIdentifier& toIdent, Value* from,CodeGenContext& context)
{
	return generateAssignmentActual(to,toIdent,from,context,true);
}


Value* CAssignment::generateAssignmentActual(BitVariable& to,const CIdentifier& toIdent, Value* from,CodeGenContext& context,bool clearImpedance)
{
	Value* assignTo;

	assignTo = to.value;

	if (toIdent.IsArray())
	{
		Value* exprResult=toIdent.GetExpression()->codeGen(context);

		Type* ty = Type::getIntNTy(getGlobalContext(),to.arraySize.getLimitedValue());
		Type* ty64 = Type::getIntNTy(getGlobalContext(),64);
		Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,ty,false);
		Instruction* truncExt0 = CastInst::Create(op,exprResult,ty,"cast",context.currentBlock());		// Cast index to 64 bit type
		Instruction::CastOps op1 = CastInst::getCastOpcode(exprResult,false,ty64,false);
		Instruction* truncExt = CastInst::Create(op1,truncExt0,ty64,"cast",context.currentBlock());		// Cast index to 64 bit type
//		const Type* ty = Type::getIntNTy(getGlobalContext(),to.arraySize.getLimitedValue());
//		Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,ty,false);
//		Instruction* truncExt = CastInst::Create(op,exprResult,ty,"cast",context.currentBlock());		// Cast index to 64 bit type

		std::vector<Value*> indices;
		ConstantInt* index0 = ConstantInt::get(getGlobalContext(), APInt(to.size.getLimitedValue(), StringRef("0"), 10));
		indices.push_back(index0);
		indices.push_back(truncExt);
		Instruction* elementPtr = GetElementPtrInst::Create(to.value,indices,"array index",context.currentBlock());

		assignTo = elementPtr;//new LoadInst(elementPtr, "", false, context.currentBlock());
	}

	if (!assignTo->getType()->isPointerTy())
	{
		std::cerr << "You cannot assign a value to a non variable (or input parameter to function)!" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	// Handle variable promotion
	Type* ty = Type::getIntNTy(getGlobalContext(),to.size.getLimitedValue());
	Instruction::CastOps op = CastInst::getCastOpcode(from,false,ty,false);

	Instruction* truncExt = CastInst::Create(op,from,ty,"cast",context.currentBlock());

	// Handle masking and constants and shift
	ConstantInt* const_intShift = ConstantInt::get(getGlobalContext(), to.shft);
	BinaryOperator* shiftInst = BinaryOperator::Create(Instruction::Shl,truncExt,const_intShift, "Shifting", context.currentBlock());
	ConstantInt* const_intMask = ConstantInt::get(getGlobalContext(), to.mask);	
	BinaryOperator* andInst = BinaryOperator::Create(Instruction::And, shiftInst, const_intMask , "Masking", context.currentBlock());

	// cnst initialiser only used when we are updating the primary register
	BinaryOperator* final;

	if (to.aliased == false)
	{
		ConstantInt* const_intCnst = ConstantInt::get(getGlobalContext(), to.cnst);
		BinaryOperator* orInst = BinaryOperator::Create(Instruction::Or, andInst, const_intCnst, "Constants", context.currentBlock());
		final=orInst;
	}
	else
	{
		Value* dest=to.value;
		if (toIdent.IsArray())
		{
			Value* exprResult=toIdent.GetExpression()->codeGen(context);

			Type* ty = Type::getIntNTy(getGlobalContext(),to.arraySize.getLimitedValue());
			Type* ty64 = Type::getIntNTy(getGlobalContext(),64);
			Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,ty,false);
			Instruction* truncExt0 = CastInst::Create(op,exprResult,ty,"cast",context.currentBlock());		// Cast index to 64 bit type
			Instruction::CastOps op1 = CastInst::getCastOpcode(exprResult,false,ty64,false);
			Instruction* truncExt = CastInst::Create(op1,truncExt0,ty64,"cast",context.currentBlock());		// Cast index to 64 bit type
	//		const Type* ty = Type::getIntNTy(getGlobalContext(),to.arraySize.getLimitedValue()/* 64*/);
	//		Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,ty,false);
	//		Instruction* truncExt = CastInst::Create(op,exprResult,ty,"cast",context.currentBlock());		// Cast index to 64 bit type

			std::vector<Value*> indices;
			ConstantInt* index0 = ConstantInt::get(getGlobalContext(), APInt(to.size.getLimitedValue(), StringRef("0"), 10));
			indices.push_back(index0);
			indices.push_back(truncExt);
			Instruction* elementPtr = GetElementPtrInst::Create(to.value,indices,"array index",context.currentBlock());

			dest = elementPtr;
		}
		// Now if the assignment is assigning to an aliased register part, we need to have loaded the original register, masked off the inverse of the section mask, and or'd in the result before we store
		LoadInst* loadInst=new LoadInst(dest, "", false, context.currentBlock());
		ConstantInt* const_intInvMask = ConstantInt::get(getGlobalContext(), ~to.mask);
		BinaryOperator* primaryAndInst = BinaryOperator::Create(Instruction::And, loadInst, const_intInvMask , "InvMasking", context.currentBlock());
		final = BinaryOperator::Create(Instruction::Or,primaryAndInst,andInst,"Combining",context.currentBlock());
	}
	
	if (to.fromExternal)
	{
		if (to.pinType!=TOK_IN && to.pinType!=TOK_BIDIRECTIONAL)
		{
			std::cerr << "Pin marked as non writable in module!" << toIdent.name << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		else
		{
			// At this point, we need to get PinSet method and call it.
			Function* function = context.LookupFunctionInExternalModule(toIdent.module,context.symbolPrepend+"PinSet"+toIdent.name);

			std::vector<Value*> args;
			args.push_back(final);
			return CallInst::Create(function,args,"",context.currentBlock());
		}

	}
	else
	{
		if (to.impedance && clearImpedance)	// clear impedance
		{
			ConstantInt* impedance = ConstantInt::get(getGlobalContext(), APInt(to.size.getLimitedValue(), StringRef("0"), 10));
			new StoreInst(impedance, to.impedance, false, context.currentBlock());
		}
		return new StoreInst(final, assignTo, false, context.currentBlock());
	}
}

void CAssignment::prePass(CodeGenContext& context)
{
	rhs.prePass(context);
}

Value* CAssignment::codeGen(CodeGenContext& context)
{
	BitVariable var;
	Value* assignWith;
	if (!context.LookupBitVariable(var,lhs.module,lhs.name))
	{
		return NULL;
	}

	if (var.mappingRef)
	{
		std::cerr << "Cannot perform operation on a mappingRef" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	if (rhs.IsImpedance())
	{
		// special case, impedance uses hidden bit variable (which is only accessed during bus muxing)	- Going to need a few fixups to support this
		if (var.pinType!=TOK_BIDIRECTIONAL)
		{
			std::cerr << "HIGH_IMPEDANCE only makes sense for BIDIRECTIONAL pins!";
			context.errorFlagged=true;
			return NULL;
		}

		return CAssignment::generateImpedanceAssignment(var,var.impedance,context);
	}
	assignWith = rhs.codeGen(context);

	return CAssignment::generateAssignment(var,lhs,assignWith,context);
}

Value* CAssignment::codeGen(CodeGenContext& context,CCastOperator* cast)
{
	BitVariable var;
	Value* assignWith;
	if (!context.LookupBitVariable(var,lhs.module,lhs.name))
	{
		return NULL;
	}

	if (var.mappingRef)
	{
		std::cerr << "Cannot perform operation on a mappingRef" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	var.aliased=true;		// We pretend we are assigning to an alias, even if we are not, this forces the compiler to generate the correct code

	APInt mask(var.size.getLimitedValue(),"0",10);
	APInt start=cast->beg.integer;
	APInt loop=cast->beg.integer.zextOrTrunc(var.size.getLimitedValue());
	APInt endloop = cast->end.integer.zextOrTrunc(var.size.getLimitedValue());
	start=start.zextOrTrunc(var.size.getLimitedValue());

	while (1==1)
	{
		mask.setBit(loop.getLimitedValue());
		if (loop.uge(endloop))
			break;
		loop++;
	}


	var.shft=start;
	var.mask=mask;

	assignWith = rhs.codeGen(context);

	return CAssignment::generateAssignment(var,lhs,assignWith,context);
}

void CBlock::prePass(CodeGenContext& context)
{
	StatementList::const_iterator it;
	for (it = statements.begin(); it != statements.end(); it++) 
	{
		(**it).prePass(context);
	}
}

Value* CBlock::codeGen(CodeGenContext& context)
{
	StatementList::const_iterator it;
	Value *last = NULL;
	for (it = statements.begin(); it != statements.end(); it++) 
	{
		last = (**it).codeGen(context);
	}
	return last;
}

void CExpressionStatement::prePass(CodeGenContext& context)
{
	expression.prePass(context);
}

Value* CExpressionStatement::codeGen(CodeGenContext& context)
{
	return expression.codeGen(context);
}

Value* CStateDeclaration::codeGen(CodeGenContext& context,Function* parent)
{
	// We need to create a bunch of labels - one for each case.. we will need a finaliser for the blocks when we pop out of our current block.
	entry = BasicBlock::Create(getGlobalContext(),id.name+"entry",parent);
	exit = BasicBlock::Create(getGlobalContext(),id.name+"exit",parent);

	return NULL;
}

void CStatesDeclaration::prePass(CodeGenContext& context)
{
	int a;

	for (a=0;a<states.size();a++)
	{
		children.push_back(NULL);
	}

	context.pushState(this);

	block.prePass(context);

	context.popState();

	if (context.currentIdent()!=NULL)
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

	if (state==NULL)
		return 1;

	for (a=0;a<state->states.size();a++)
	{
		CStatesDeclaration* downState=state->children[a];

		if (downState==NULL)
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

	if (cur==NULL)
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

		if (downState==NULL)
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

	if (context.currentState()==NULL)
	{
		if (context.parentHandler==NULL)
		{
			std::cerr << "It is illegal to declare STATES outside of a handler!" << std::endl;
			context.errorFlagged=true;
			return NULL;
		}

		TopMostState=true;
	}
	else
	{
	}

	int totalStates;
	Twine numStatesTwine;
	std::string numStates;
	APInt overSized;
	unsigned bitsNeeded;
	int baseStateIdx;
	
	Value *curState;
	Value *nxtState;
	std::string stateLabel = "STATE" + context.stateLabelStack;

	Value* int32_5=NULL;
	if (TopMostState)
	{
		totalStates = GetNumStates(this);
		numStatesTwine=Twine(totalStates);
		numStates = numStatesTwine.str();
		overSized=APInt(4*numStates.length(),numStates,10);
		bitsNeeded = overSized.getActiveBits();
		baseStateIdx=0;

		std::string curStateLbl = "CUR" + context.stateLabelStack;
		std::string nxtStateLbl = "NEXT" + context.stateLabelStack;
		std::string idxStateLbl = "IDX" + context.stateLabelStack;
		std::string stkStateLbl = "STACK" + context.stateLabelStack;

		GlobalVariable* gcurState = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),bitsNeeded), false, GlobalValue::PrivateLinkage,NULL,context.symbolPrepend+curStateLbl);
		GlobalVariable* gnxtState = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),bitsNeeded), false, GlobalValue::PrivateLinkage,NULL,context.symbolPrepend+nxtStateLbl);

		curState = gcurState;
		nxtState = gnxtState;

		ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(getGlobalContext(), bitsNeeded), MAX_SUPPORTED_STACK_DEPTH);
		GlobalVariable* stkState = new GlobalVariable(*context.module, ArrayTy_0, false,  GlobalValue::PrivateLinkage, NULL, context.symbolPrepend+stkStateLbl);
		GlobalVariable *stkStateIdx = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),MAX_SUPPORTED_STACK_BITS), false, GlobalValue::PrivateLinkage,NULL,context.symbolPrepend+idxStateLbl);

		StateVariable newStateVar;
		newStateVar.currentState = curState;
		newStateVar.nextState = nxtState;
		newStateVar.stateStackNext = stkState;
		newStateVar.stateStackIndex = stkStateIdx;
		newStateVar.decl = this;

		context.states()[stateLabel]=newStateVar;
		context.statesAlt()[this]=newStateVar;

		// Constant Definitions
		ConstantInt* const_int32_n = ConstantInt::get(getGlobalContext(), APInt(bitsNeeded, 0, false));
		ConstantInt* const_int4_n = ConstantInt::get(getGlobalContext(), APInt(MAX_SUPPORTED_STACK_BITS, 0, false));
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
		numStatesTwine=Twine(totalStates);
		numStates = numStatesTwine.str();
		overSized=APInt(4*numStates.length(),numStates,10);
		bitsNeeded = overSized.getActiveBits();

		baseStateIdx=ComputeBaseIdx(topState.decl,this);

		int32_5 = topState.decl->optocurState;
	}
	BasicBlock* bb = context.currentBlock();		// cache old basic block

	std::map<std::string,BitVariable> tmp = context.locals();

	// Setup exit from switch statement
	exitState = BasicBlock::Create(getGlobalContext(),"switchTerm",bb->getParent());
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
			ConstantInt* tt = ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,startIdx+b,false));
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
			if (children[a]==NULL)
			{
				ConstantInt* nextState = ConstantInt::get(getGlobalContext(),APInt(bitsNeeded, a==states.size()-1 ? baseStateIdx : startIdx,false));
				StoreInst* newState = new StoreInst(nextState,nxtState,false,states[a]->entry);
			}
		}
		else
		{
			if (children[a]==NULL)
			{
				ConstantInt* nextState = ConstantInt::get(getGlobalContext(),APInt(bitsNeeded, startOfAutoIncrementIdx,false));
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
	
	return NULL;
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
		else
		{
			std::cerr << "Attempt to define unknown state : " << id.name.c_str() << endl;
			context.errorFlagged=true;
		}
	}
	else
	{
		std::cerr << "Attempt to define state entry when no states on stack" << endl;
		context.errorFlagged=true;
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
			context.pushBlock(pState->entry);
			block.codeGen(context);
			pState->entry=context.currentBlock();
			context.popBlock();
		}
		else
		{
			std::cerr << "Attempt to define unknown state : " << id.name.c_str() << endl;
			context.errorFlagged=true;
			return NULL;
		}
	}
	else
	{
		std::cerr << "Attempt to define state entry when no states on stack" << endl;
		context.errorFlagged=true;
		return NULL;
	}
}


void CVariableDeclaration::CreateWriteAccessor(CodeGenContext& context,BitVariable& var,const std::string& moduleName,const std::string& name,bool impedance)
{
	vector<Type*> argTypes;
	argTypes.push_back(IntegerType::get(getGlobalContext(), var.size.getLimitedValue()));
	FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext()),argTypes, false);
	Function* function;
	if (context.isRoot)
	{
		function = Function::Create(ftype, GlobalValue::ExternalLinkage, context.symbolPrepend+"PinSet"+id.name, context.module);
	}
	else
	{
		function = Function::Create(ftype, GlobalValue::PrivateLinkage, context.symbolPrepend+"PinSet"+id.name, context.module);
	}
	function->onlyReadsMemory(0);	// Mark input read only
	function->setDoesNotThrow();
	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", function, 0);

	context.pushBlock(bblock);

	Function::arg_iterator args = function->arg_begin();
	Value* setVal=args;
	setVal->setName("InputVal");

	LoadInst* load=new LoadInst(var.value,"",false,bblock);

    if (impedance)
	{
		LoadInst* loadImp = new LoadInst(var.impedance,"",false,bblock);
		CmpInst* check=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,loadImp,ConstantInt::get(getGlobalContext(),APInt(var.size.getLimitedValue(),0)),"impedance",bblock);

		setVal = SelectInst::Create(check,setVal,load,"impOrReal",bblock);
	}

	Value* stor=CAssignment::generateAssignmentActual(var,id/*moduleName, name*/,setVal,context,false);		// we shouldn't clear impedance on write via pin (instead should ignore write if impedance is set)

	var.priorValue=load;
	var.writeInput=setVal;
	var.writeAccessor=&writeAccessor;
	writeAccessor=ReturnInst::Create(getGlobalContext(),bblock);

	context.popBlock();
}

void CVariableDeclaration::CreateReadAccessor(CodeGenContext& context,BitVariable& var,bool impedance)
{
	vector<Type*> argTypes;
	FunctionType *ftype = FunctionType::get(IntegerType::get(getGlobalContext(), var.size.getLimitedValue()),argTypes, false);
	Function* function;
    if (context.isRoot)
	{
		function = Function::Create(ftype, GlobalValue::ExternalLinkage, context.symbolPrepend+"PinGet"+id.name, context.module);
	}
	else
	{
		function = Function::Create(ftype, GlobalValue::PrivateLinkage, context.symbolPrepend+"PinGet"+id.name, context.module);
	}
	function->setOnlyReadsMemory();
	function->setDoesNotThrow();

	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", function, 0);

	Value* load = new LoadInst(var.value,"",false,bblock);
    if (impedance)
	{
		LoadInst* loadImp = new LoadInst(var.impedance,"",false,bblock);
		CmpInst* check=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,loadImp,ConstantInt::get(getGlobalContext(),APInt(var.size.getLimitedValue(),0)),"impedance",bblock);

		load = SelectInst::Create(check,load,loadImp,"impOrReal",bblock);
	}

	ReturnInst::Create(getGlobalContext(),load,bblock);
}

void CVariableDeclaration::prePass(CodeGenContext& context)
{
	// Do nothing, but in future we could pre compute the globals and the locals for each block, allowing use before declaration?
}

Value* CVariableDeclaration::codeGen(CodeGenContext& context)
{
	BitVariable temp;

	temp.arraySize = arraySize.integer;
	temp.size = size.integer;
	temp.trueSize = size.integer;
	temp.cnst = APInt(size.integer.getLimitedValue(),0);
	temp.mask = ~temp.cnst;
	temp.shft = APInt(size.integer.getLimitedValue(),0);
	temp.aliased = false;
	temp.mappingRef = false;
	temp.pinType=pinType;
	temp.writeAccessor=NULL;
	temp.writeInput=NULL;
	temp.priorValue=NULL;
	temp.impedance=NULL;
	temp.fromExternal=false;

	if (context.currentBlock())
	{
		if (pinType!=0)
		{
			std::cerr << "Cannot declare pins anywhere but global scope" << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		// Within a basic block - so must be a stack variable
		AllocaInst *alloc = new AllocaInst(Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()), id.name.c_str(), context.currentBlock());
		temp.value = alloc;
	}
	else
	{
		// GLobal variable
		// Rules for globals have changed. If we are definining a PIN then the variable should be private to this module, and accessors should be created instead. 
		if (pinType==0)
		{
			Type* type = Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue());
			if (arraySize.integer.getLimitedValue())
			{
				APInt power2(arraySize.integer.getLimitedValue()+1,1);
				power2<<=arraySize.integer.getLimitedValue();
				type=ArrayType::get(Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()),power2.getLimitedValue());
			}

			if (internal /*|| !context.isRoot*/)
			{
				temp.value = new GlobalVariable(*context.module,type, false, GlobalValue::PrivateLinkage,NULL,context.symbolPrepend+id.name);
			}
			else
			{
				temp.value = new GlobalVariable(*context.module,type, false, GlobalValue::ExternalLinkage,NULL,context.symbolPrepend+id.name);
			}
		}
		else
		{
			temp.value = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()), false, GlobalValue::PrivateLinkage,NULL,context.symbolPrepend+id.name);
			switch (pinType)
			{
				case TOK_IN:
					CreateWriteAccessor(context,temp,id.module,id.name,false);
					break;
				case TOK_OUT:
					CreateReadAccessor(context,temp,false);
					break;
				case TOK_BIDIRECTIONAL:

					// In the future we should try to detect if a pin uses impedance and avoid the additional overhead

					// we also need a new variable to hold the impedance mask
					temp.impedance=new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()), false, GlobalValue::PrivateLinkage,NULL,context.symbolPrepend+id.name+".HZ");

					CreateWriteAccessor(context,temp,id.module,id.name,true);
					CreateReadAccessor(context,temp,true);
					break;
				default:
					std::cerr << "Unhandled pin type" << std::endl;
					context.errorFlagged=true;
					return NULL;
			}
		}

	}

	APInt bitPos = size.integer -1;
	for (int a=0;a<aliases.size();a++)
	{
		if (aliases[a]->idOrEmpty.name.size()<1)
		{
			// For constants we need to update the mask and cnst values (we also need to set the initialiser properly - that will come later)

			APInt newMask = ~APInt(aliases[a]->sizeOrValue.integer.getBitWidth(),0);
			APInt newCnst = aliases[a]->sizeOrValue.integer;

			bitPos-=APInt(bitPos.getBitWidth(),aliases[a]->sizeOrValue.integer.getBitWidth()-1);

			if (newMask.getBitWidth()!=size.integer.getLimitedValue())
			{
	    			newMask=newMask.zext(size.integer.getLimitedValue());
			}
			if (newCnst.getBitWidth()!=size.integer.getLimitedValue())
			{
	    			newCnst=newCnst.zext(size.integer.getLimitedValue());
			}
	    		newCnst<<=bitPos.getLimitedValue();
	    		newMask<<=bitPos.getLimitedValue();
	    		temp.mask&=~newMask;
	    		temp.cnst|=newCnst;

			bitPos-=APInt(bitPos.getBitWidth(),1);
		}
		else
		{
			// For identifiers, we need to create another entry in the correct symbol table and set up the masks appropriately
			// e.g. BOB[4] ALIAS CAT[2]:%01
			// would create CAT with a shift of 2 a mask of %1100 (cnst will always be 0 for these)

			bitPos-=APInt(bitPos.getBitWidth(),aliases[a]->sizeOrValue.integer.getLimitedValue()-1);

			BitVariable alias;
			alias.arraySize = temp.arraySize;
			alias.size = temp.size;
			alias.trueSize = aliases[a]->sizeOrValue.integer;
			alias.value = temp.value;	// the value will always point at the stored local/global
			alias.cnst = temp.cnst;		// ignored
			alias.mappingRef=false;
			alias.pinType=temp.pinType;
			alias.writeAccessor=temp.writeAccessor;
			alias.writeInput=temp.writeInput;
			alias.priorValue=temp.priorValue;
			alias.impedance=NULL;

			APInt newMask = ~APInt(aliases[a]->sizeOrValue.integer.getLimitedValue(),0);
			if (newMask.getBitWidth()!=size.integer.getLimitedValue())
			{
	    			newMask=newMask.zext(size.integer.getLimitedValue());
			}
	    		newMask<<=bitPos.getLimitedValue();
			alias.mask = newMask;		// need to generate correct mask

			alias.shft = APInt(size.integer.getLimitedValue(),bitPos.getLimitedValue());
			alias.aliased=true;

			if (context.currentBlock())
			{
				context.locals()[aliases[a]->idOrEmpty.name] = alias;
			}
			else
			{
				context.globals()[aliases[a]->idOrEmpty.name]= alias;
			}

			bitPos-=APInt(bitPos.getBitWidth(),1);
		}
	}

	ConstantInt* const_intn_0 = ConstantInt::get(getGlobalContext(), temp.cnst);
	if (context.currentBlock())
	{
		// Initialiser Definitions

		if (initialiserList.empty())
		{
			StoreInst* stor = new StoreInst(const_intn_0,temp.value,false,context.currentBlock());		// NOT SURE HOW WELL THIS WILL WORK IN FUTURE
		}
		else
		{
			if (initialiserList.size()!=1)
			{
				std::cerr << "Too many initialisers" << endl;
				context.errorFlagged=true;
				return NULL;
			}

			APInt t = initialiserList[0]->integer;

			if (t.getBitWidth()!=size.integer.getLimitedValue())
			{
				t = t.zext(size.integer.getLimitedValue());
			}

			ConstantInt* constInit=ConstantInt::get(getGlobalContext(),t);

			StoreInst* stor = new StoreInst(constInit,temp.value,false,context.currentBlock());
		}
		context.locals()[id.name] = temp;
	}
	else
	{
		// Initialiser Definitions
		if (temp.arraySize.getLimitedValue())
		{
			APInt power2(arraySize.integer.getLimitedValue()+1,1);
			power2<<=arraySize.integer.getLimitedValue();

			if (initialiserList.empty())
			{
				ConstantAggregateZero* const_array_7 = ConstantAggregateZero::get(ArrayType::get(Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()),power2.getLimitedValue()));
				cast<GlobalVariable>(temp.value)->setInitializer(const_array_7);
			}
			else
			{
				if (initialiserList.size()>power2.getLimitedValue())
				{
					std::cerr << "Too many initialisers" << endl;
					context.errorFlagged=true;
					return NULL;
				}
		
				int a;
				std::vector<Constant*> const_array_9_elems;
				ArrayType* arrayTy = ArrayType::get(Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()),power2.getLimitedValue());
				ConstantInt* const0 = ConstantInt::get(getGlobalContext(), APInt(size.integer.getLimitedValue(),0));

				for (a=0;a<power2.getLimitedValue();a++)
				{
					if (a<initialiserList.size())
					{
						APInt t = initialiserList[a]->integer;
			
						if (t.getBitWidth()!=size.integer.getLimitedValue())
						{
							t = t.zext(size.integer.getLimitedValue());
						}

						const_array_9_elems.push_back(ConstantInt::get(getGlobalContext(),t));
					}
					else
					{
						const_array_9_elems.push_back(const0);
					}
				}

				Constant* const_array_9 = ConstantArray::get(arrayTy, const_array_9_elems);
				cast<GlobalVariable>(temp.value)->setInitializer(const_array_9);
			}
		}
		else
		{
			if (initialiserList.empty())
			{
				cast<GlobalVariable>(temp.value)->setInitializer(const_intn_0);
				if (temp.impedance)
				{
					cast<GlobalVariable>(temp.impedance)->setInitializer(const_intn_0);
				}
			}
			else
			{
				if (initialiserList.size()!=1)
				{
					std::cerr << "Too many initialisers" << endl;
					context.errorFlagged=true;
					return NULL;
				}

				APInt t = initialiserList[0]->integer;
			
				if (t.getBitWidth()!=size.integer.getLimitedValue())
				{
					t = t.zext(size.integer.getLimitedValue());
				}

				ConstantInt* constInit=ConstantInt::get(getGlobalContext(),t);
				cast<GlobalVariable>(temp.value)->setInitializer(constInit);
			}
		}

		context.globals()[id.name]=temp;
	}

	return temp.value;
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
		std::cerr << "undeclared pin " << id.name << " - Handlers MUST be tied to pin definitions!" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	pinVariable=context.globals()[id.name];

	if (pinVariable.writeAccessor==NULL)
	{
		std::cerr << "Handlers must be tied to Input / Bidirectional pins ONLY! - " << id.name << std::endl;
	}

	FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext()),argTypes, false);
	Function* function = Function::Create(ftype, GlobalValue::PrivateLinkage, context.symbolPrepend+"HANDLER."+id.name, context.module);
	function->setDoesNotThrow();
	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", function, 0);
	
	context.m_handlers[id.name]=this;

	context.pushBlock(bblock);

	std::string oldStateLabelStack = context.stateLabelStack;
	context.stateLabelStack+="." + id.name;

	context.parentHandler=this;

	trigger.codeGen(context,pinVariable,function);

	block.codeGen(context);

	context.parentHandler=NULL;

	ReturnInst::Create(getGlobalContext(), context.currentBlock());			/* block may well have changed by time we reach here */

	context.stateLabelStack=oldStateLabelStack;

	context.popBlock();

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
		if (strchr(okChars,in[a])==NULL)
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
		std::cerr << "Opcode for instruction must be able to generate constants" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}
			
//	std::cout << "Adding ins " << numOpcodes << std::endl;

	for (unsigned a=0;a<numOpcodes;a++)
	{
		APInt opcode = operands[0]->GetComputableConstant(context,a);

		CString disassembled=processMnemonic(context,mnemonic,opcode,a);
		CDebugTraceString opcodeString(disassembled);

		context.disassemblyTable[table.name][opcode]=disassembled.quoted;

		FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext()),argTypes, false);
		Function* function = Function::Create(ftype, GlobalValue::PrivateLinkage, EscapeString(context.symbolPrepend+"OPCODE_"+opcodeString.string.quoted.substr(1,opcodeString.string.quoted.length()-2) + "_" + table.name+opcode.toString(16,false)),context.module);
		function->setDoesNotThrow();
		BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", function, 0);

		context.pushBlock(bblock);

		operands[0]->DeclareLocal(context,a);

		block.codeGen(context);

		ReturnInst::Create(getGlobalContext(), context.currentBlock());			/* block may well have changed by time we reach here */

		context.popBlock();

		// Glue callee back into execute (assumes execute comes before instructions at all times for now)
		for (int b=0;b<context.executeLocations[table.name].size();b++)
		{
//			std::cout << "Adding execute " << opcode.toString(2,false) << std::endl;
			BasicBlock* tempBlock = BasicBlock::Create(getGlobalContext(),"callOut" + table.name + opcode.toString(16,false),context.executeLocations[table.name][b].blockEndForExecute->getParent(),0);
			std::vector<Value*> args;
			CallInst* fcall = CallInst::Create(function,args,"",tempBlock);
			BranchInst::Create(context.executeLocations[table.name][b].blockEndForExecute,tempBlock);
			context.executeLocations[table.name][b].switchForExecute->addCase(ConstantInt::get(getGlobalContext(),opcode),tempBlock);
		}

	}
	return NULL;
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

		temp.blockEndForExecute = BasicBlock::Create(getGlobalContext(), "execReturn", context.currentBlock()->getParent(), 0);		// Need to cache this block away somewhere
	
		if (context.opts.traceUnimplemented)
		{
			BasicBlock* tempBlock=BasicBlock::Create(getGlobalContext(),"default",context.currentBlock()->getParent(),0);

			std::vector<Value*> args;
	
			// Handle variable promotion
			Type* ty = Type::getIntNTy(getGlobalContext(),32);
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
	return NULL;
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
				if (downState==NULL)
				{
					total=1;
				}
				else
				{
					total=ComputeBaseIdx(downState,list,0,total);
				}
				return totalStates;
			}

			if (downState==NULL)
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

			if (downState==NULL)
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
		std::cerr << "Unknown handler : " << stateIdents[0]->name << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates=GetNumStates(topState.decl);
	int totalInBlock;

	int jumpIndex=ComputeBaseIdx(topState.decl,stateIdents,1,totalInBlock);
	
	if (jumpIndex==-1)
	{
		std::cerr << "Unknown state requested!";
		for (int a=0;a<stateIdents.size();a++)
		{
			std::cerr << stateIdents[a]->name;
			if (a!=stateIdents.size()-1)
			{
				std::cerr << ".";
			}
		}
		std::cerr << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();

	// Load value from state variable, test against range of values
	Value* load = new LoadInst(topState.currentState, "", false, context.currentBlock());

	if (totalInBlock>1)
	{
		CmpInst* cmp = CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_UGE,load, ConstantInt::get(getGlobalContext(), APInt(bitsNeeded,jumpIndex)), "", context.currentBlock());
		CmpInst* cmp2 = CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_ULT,load, ConstantInt::get(getGlobalContext(), APInt(bitsNeeded,jumpIndex+totalInBlock)), "", context.currentBlock());
		return BinaryOperator::Create(Instruction::And,cmp,cmp2,"Combining",context.currentBlock());
	}
		
	// Load value from state variable, test against being equal to found index, jump on result
	return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,load, ConstantInt::get(getGlobalContext(), APInt(bitsNeeded,jumpIndex)), "", context.currentBlock());
}

void CStateJump::prePass(CodeGenContext& context)
{
	
}

Value* CStateJump::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE."+stateIdents[0]->name;

	if (context.states().find(stateLabel)==context.states().end())
	{
		std::cerr << "Unknown handler : " << stateIdents[0]->name << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates=GetNumStates(topState.decl);
	int totalInBlock;

	int jumpIndex=ComputeBaseIdx(topState.decl,stateIdents,1,totalInBlock);
	
	if (jumpIndex==-1)
	{
		std::cerr << "Unknown state requested!";
		for (int a=0;a<stateIdents.size();a++)
		{
			std::cerr << stateIdents[a]->name;
			if (a!=stateIdents.size()-1)
			{
				std::cerr << ".";
			}
		}
		std::cerr << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();
	
	return new StoreInst(ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,jumpIndex)),topState.nextState,false,context.currentBlock());
}

void CStatePush::prePass(CodeGenContext& context)
{
	
}


Value* CStatePush::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE."+stateIdents[0]->name;

	if (context.states().find(stateLabel)==context.states().end())
	{
		std::cerr << "Unknown handler : " << stateIdents[0]->name << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	StateVariable topState = context.states()[stateLabel];

	int totalStates=GetNumStates(topState.decl);
	int totalInBlock;

	int jumpIndex=ComputeBaseIdx(topState.decl,stateIdents,1,totalInBlock);
	
	if (jumpIndex==-1)
	{
		std::cerr << "Unknown state requested!";
		for (int a=0;a<stateIdents.size();a++)
		{
			std::cerr << stateIdents[a]->name;
			if (a!=stateIdents.size()-1)
			{
				std::cerr << ".";
			}
		}
		std::cerr << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	Twine numStatesTwine(totalStates);
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();
	
	Value* index= new LoadInst(topState.stateStackIndex,"stackIndex",false,context.currentBlock());
	std::vector<Value*> indices;
	ConstantInt* const_intn = ConstantInt::get(getGlobalContext(), APInt(bitsNeeded, StringRef("0"), 10));
	indices.push_back(const_intn);
	indices.push_back(index);
	Value* ref = GetElementPtrInst::Create(topState.stateStackNext, indices,"stackPos",context.currentBlock());

	Value* curNext = new LoadInst(topState.nextState,"currentNext",false,context.currentBlock());

	new StoreInst(curNext,ref,false,context.currentBlock());			// Save current next point to stack

	Value* inc = BinaryOperator::Create(Instruction::Add,index,ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,1)),"incrementIndex",context.currentBlock());
	new StoreInst(inc,topState.stateStackIndex,false,context.currentBlock());	// Save updated index

	// Set next state
	return new StoreInst(ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,jumpIndex)),topState.nextState,false,context.currentBlock());
}
	
void CStatePop::prePass(CodeGenContext& context)
{
	
}


Value* CStatePop::codeGen(CodeGenContext& context)
{
	std::string stateLabel = "STATE."+ident.name;

	if (context.states().find(stateLabel)==context.states().end())
	{
		std::cerr << "Unknown handler : " << ident.name << std::endl;
		context.errorFlagged=true;
		return NULL;
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
	Value* dec = BinaryOperator::Create(Instruction::Sub,index,ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,1)),"decrementIndex",context.currentBlock());
	new StoreInst(dec,topState.stateStackIndex,false,context.currentBlock());	// store new stack index

	std::vector<Value*> indices;
	ConstantInt* const_intn = ConstantInt::get(getGlobalContext(), APInt(bitsNeeded, StringRef("0"), 10));
	indices.push_back(const_intn);
	indices.push_back(dec);
	Value* ref = GetElementPtrInst::Create(topState.stateStackNext, indices,"stackPos",context.currentBlock());

	Value* oldNext = new LoadInst(ref,"oldNext",false,context.currentBlock());	// retrieve old next value

	return new StoreInst(oldNext,topState.nextState,false,context.currentBlock());	// restore next state to old value
}

void CIfStatement::prePass(CodeGenContext& context)
{
	block.prePass(context);
}

Value* CIfStatement::codeGen(CodeGenContext& context)
{
	BasicBlock *then = BasicBlock::Create(getGlobalContext(),"then",context.currentBlock()->getParent());
	BasicBlock *endif = BasicBlock::Create(getGlobalContext(),"endif",context.currentBlock()->getParent());

	Value* result = expr.codeGen(context);
	BranchInst::Create(then,endif,result,context.currentBlock());

	context.pushBlock(then);

	block.codeGen(context);
	BranchInst::Create(endif,context.currentBlock());

	context.popBlock();

	context.setBlock(endif);

	return NULL;
}

void COperandIdent::DeclareLocal(CodeGenContext& context,unsigned num)
{
	BitVariable temp;

	temp.size = size.integer;
	temp.trueSize = size.integer;
	temp.cnst = APInt(size.integer.getLimitedValue(),num);
	temp.mask = APInt(size.integer.getLimitedValue(),0);
	temp.shft = APInt(size.integer.getLimitedValue(),0);
	temp.aliased = false;
	temp.mappingRef=false;
	temp.pinType=0;
	temp.writeAccessor=NULL;
	temp.writeInput=NULL;
	temp.priorValue=NULL;
	temp.impedance=NULL;

	AllocaInst *alloc = new AllocaInst(Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()), ident.name.c_str(), context.currentBlock());
	temp.value = alloc;

	ConstantInt* const_intn_0 = ConstantInt::get(getGlobalContext(), temp.cnst);
	StoreInst* stor = new StoreInst(const_intn_0,temp.value,false,context.currentBlock());		// NOT SURE HOW WELL THIS WILL WORK IN FUTURE
		
	context.locals()[ident.name] = temp;
}
	
void COperandMapping::DeclareLocal(CodeGenContext& context,unsigned num)
{
	context.locals()[ident.name] = GetBitVariable(context,num);
}

BitVariable COperandMapping::GetBitVariable(CodeGenContext& context,unsigned num)
{
	BitVariable temp;

	if (context.m_mappings.find(ident.name)==context.m_mappings.end())
	{
		std::cerr << "Undeclared mapping : " << ident.name << std::endl;
		context.errorFlagged=true;
		return temp;
	}

	CMapping* mapping = context.m_mappings[ident.name]->mappings[num];

	if (mapping->expr.IsIdentifierExpression())
	{
		// If the expression in a mapping is a an identifer alone, then we can create a true local variable that maps directly to it, this allows
		// assignments to work. (its done by copying the variable declaration resulting from looking up the identifier.

		CIdentifier* identifier = (CIdentifier*)&mapping->expr;

		if (!context.LookupBitVariable(temp,identifier->module,identifier->name))
		{
			return temp;
		}
	}
	else
	{
		// Not an identifier, so we must create an expression map (this limits the mapping to only being useful for a limited set of things, (but is an accepted part of the language!)

		temp.mappingRef=true;
		temp.mapping = &mapping->expr;
	}
}

llvm::APInt COperandMapping::GetComputableConstant(CodeGenContext& context,unsigned num)
{
	APInt error;

	if (context.m_mappings.find(ident.name)==context.m_mappings.end())
	{
		std::cerr << "Undeclared mapping : " << ident.name << std::endl;
		context.errorFlagged=true;
		return error;
	}

	return context.m_mappings[ident.name]->mappings[num]->selector.integer;
}

unsigned COperandMapping::GetNumComputableConstants(CodeGenContext& context)
{
	if (context.m_mappings.find(ident.name)==context.m_mappings.end())
	{
		std::cerr << "Undeclared mapping : " << ident.name << std::endl;
		context.errorFlagged=true;
		return 0;
	}

	return context.m_mappings[ident.name]->mappings.size();
}
	
const CString* COperandMapping::GetString(CodeGenContext& context,unsigned num,unsigned slot)
{
	if (context.m_mappings.find(ident.name)==context.m_mappings.end())
	{
		std::cerr << "Undeclared mapping : " << ident.name << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	return &context.m_mappings[ident.name]->mappings[num]->label;
}

bool COperandPartial::isStringReturnable()
{
	for (int a=0;a<operands.size();a++)
	{
		if (operands[a]->isStringReturnable())
		{
			return true;
		}
	}

	return false;
}

const CString* COperandPartial::GetString(CodeGenContext& context,unsigned num,unsigned slot)
{
	int a;
	int cnt=0;
	// BUGFIX: string generation params are designed to be forward iterated, but the partial loops
	//go backwards. This fixes the problem

	for (a=0;a<operands.size();a++)
	{
		if (operands[a]->isStringReturnable())
		{
			cnt++;
		}
	}

	slot=(cnt-slot)-1;
	for (a=operands.size()-1;a>=0;a--)
	{
		unsigned tNum=num % operands[a]->GetNumComputableConstants(context);
		const CString* temp= operands[a]->GetString(context,tNum,slot);
		if (temp!=NULL)
		{
			if (slot==0)
				return temp;
			slot--;
		}
		num/=operands[a]->GetNumComputableConstants(context);
	}

	return NULL;
}

void COperandPartial::DeclareLocal(CodeGenContext& context,unsigned num)
{
	for (int a=operands.size()-1;a>=0;a--)
	{
		unsigned tNum=num % operands[a]->GetNumComputableConstants(context);
		operands[a]->DeclareLocal(context,tNum);
		num/=operands[a]->GetNumComputableConstants(context);
	}
}

APInt COperandPartial::GetComputableConstant(CodeGenContext& context,unsigned num)
{
//	APInt result(0,0,false);		// LLVM 3.4 (or somewhere between 3.0 and 3.4) it became illegal to do this... The smallest size is now 1 bit. To deal with this I`ve changed the logic
//						// so that the first real value created is done explicitly

	APInt result;
	bool firstTime=true;

	if (operands.size()==0)
	{
		std::cerr << "Illegal (0) number of operands for computable constant" << std::endl;
		context.errorFlagged=true;
		return result;
	}
	for (int a=operands.size()-1;a>=0;a--)
	{
		unsigned tNum=num % operands[a]->GetNumComputableConstants(context);
		APInt temp = operands[a]->GetComputableConstant(context,tNum);
		num/=operands[a]->GetNumComputableConstants(context);
		if (firstTime)
		{
			result=temp;
			firstTime=false;
		}
		else
		{
			unsigned curBitWidth = result.getBitWidth();

			result=result.zext(result.getBitWidth()+temp.getBitWidth());
			temp=temp.zext(result.getBitWidth());
			temp=temp.shl(curBitWidth);
			result=result.Or(temp);
		}
	}

	return result;
}

unsigned COperandPartial::GetNumComputableConstants(CodeGenContext& context)
{
	unsigned result=1;

	for (int a=0;a<operands.size();a++)
	{
		result*=operands[a]->GetNumComputableConstants(context);
	}

	return result;
}

void CMappingDeclaration::prePass(CodeGenContext& context)
{

}

Value* CMappingDeclaration::codeGen(CodeGenContext& context)
{
	if (context.m_mappings.find(ident.name)!=context.m_mappings.end())
	{
		std::cerr << "Multiple declaration for mapping : " << ident.name << std::endl;
		context.errorFlagged=true;
		return NULL;
	}
	context.m_mappings[ident.name]=this;

	return NULL;
}

void CConnectDeclaration::prePass(CodeGenContext& context)
{

}

Value* CConnectDeclaration::codeGen(CodeGenContext& context)
{
  	std::vector<Type*> FuncTy_8_args;
	FunctionType* FuncTy_8;
	Function* func = NULL;

	// 1 argument at present, same as the ident - contains a single bit
	FuncTy_8_args.push_back(IntegerType::get(getGlobalContext(), 1));

	FuncTy_8 = FunctionType::get(Type::getVoidTy(getGlobalContext()),FuncTy_8_args,false);

	func=Function::Create(FuncTy_8,GlobalValue::ExternalLinkage,context.symbolPrepend+ident.name,context.module);
	func->setCallingConv(CallingConv::C);
	func->setDoesNotThrow();
	
	context.m_externFunctions[ident.name] = func;

	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", func, 0);
	
	context.pushBlock(bblock);

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
		temp.writeAccessor=NULL;
		temp.writeInput=NULL;
		temp.priorValue=NULL;
		temp.impedance=NULL;
		temp.fromExternal=false;

		temp.value=args;
		temp.value->setName(ident.name);
		context.locals()[ident.name]=temp;
		args++;
	}

	for (size_t a=0;a<connects.size();a++)
	{
		int outCnt=0,inCnt=0,biCnt=0;

		std::cerr << "Connection " << a << std::endl;
	
		// Do a quick pass to build a list of Inputs and Outputs - Maintaining declaration order (perform some validation as we go)
		std::vector<CIdentifier*> ins;
		std::vector<CExpression*> outs;

		for (size_t b=0;b<connects[a]->size();b++)
		{
			if ((*connects[a])[b]->IsIdentifierExpression())
			{
				CIdentifier* ident = (CIdentifier*)(*connects[a])[b];

				BitVariable var;
				
				if (!context.LookupBitVariable(var,ident->module,ident->name))
				{
					std::cerr << "Unknown Ident : " << ident->name << std::endl;
					context.errorFlagged=true;
					return NULL;
				}
				else
				{
					switch (var.pinType)
					{
						case 0:
							std::cerr << "Variable : Acts as Output " << ident->name << std::endl;
							outs.push_back(ident);
							outCnt++;
							break;
						case TOK_IN:
							std::cerr << "PIN : Acts as Input " << ident->name << std::endl;
							ins.push_back(ident);
							inCnt++;
							break;
						case TOK_OUT:
							std::cerr << "PIN : Acts as Output " << ident->name << std::endl;
							outs.push_back(ident);
							outCnt++;
							break;
						case TOK_BIDIRECTIONAL:
							std::cerr << "PIN : Acts as Input/Output " << ident->name << std::endl;
							ins.push_back(ident);
							outs.push_back(ident);
							biCnt++;
							break;
					}
				}
			}
			else
			{
				std::cerr << "Complex Expression : Acts as Output " << std::endl;
				outs.push_back((*connects[a])[b]);
				outCnt++;
			}
		}

		if (outCnt==0 && inCnt==0 && biCnt==0)
		{
			std::cerr << "No possible routing! - no connections" << std::endl;
			context.errorFlagged=true;
			return NULL;
		}

		if (outCnt==0 && inCnt>0 && biCnt==0)
		{
			std::cerr << "No possible routing! - all connections are input" << std::endl;
			context.errorFlagged=true;
			return NULL;
		}

		if (outCnt>0 && inCnt==0 && biCnt==0)
		{
			std::cerr << "No possible routing! - all connections are output" << std::endl;
			context.errorFlagged=true;
			return NULL;
		}

		std::cerr << "Total Connections : in " << inCnt << " , out " << outCnt << " , bidirect " << biCnt << std::endl;

		// Validation in/out done... next combine all outputs into a single bus.. then write bus to all inputs -- currently assuming pullup

		BitVariable busIVar;

		busIVar.size = 1;
		busIVar.trueSize = 1;
		busIVar.cnst = APInt(1,0);
		busIVar.mask = ~busIVar.cnst;
		busIVar.shft = APInt(1,0);
		busIVar.aliased = false;
		busIVar.mappingRef = false;
		busIVar.pinType=0;
		busIVar.writeAccessor=NULL;
		busIVar.writeInput=NULL;
		busIVar.priorValue=NULL;
		busIVar.impedance=NULL;
		busIVar.fromExternal=false;

		busIVar.value=new AllocaInst(Type::getIntNTy(getGlobalContext(),1), "bus_collect", context.currentBlock());
		busIVar.value->setName("busCombine");

//		context.locals()[std::string("busI_")+a]=temp;		// no need to add to locals

		for (size_t o=0;o<outs.size();o++)
		{
			Value* tmp = outs[o]->codeGen(context);
			if (o==0)
			{
				CAssignment::generateAssignment(busIVar,ident,tmp,context);
			}
			else
			{
				Value* t = new LoadInst(busIVar.value,"",context.currentBlock());
				t= BinaryOperator::Create(Instruction::And,tmp,t,"PullUpCombine",context.currentBlock());

				CAssignment::generateAssignment(busIVar,ident,t,context);
			}
		}

		for (size_t i=0;i<ins.size();i++)
		{
			BitVariable var;

			if (!context.LookupBitVariable(var,ins[i]->module,ins[i]->name))
			{
				std::cerr << "Failed to retrieve variable" << std::endl;
				context.errorFlagged=true;
				return NULL;
			}
			CAssignment::generateAssignment(var,*ins[i],busIVar.value,context);
		}
	}

	ReturnInst::Create(getGlobalContext(), context.currentBlock());			/* block may well have changed by time we reach here */

	context.popBlock();

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
					std::cerr << "Bit to carry is outside of range for result" << std::endl;
					context.errorFlagged=true;
					return NULL;
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
					Value* cmpResult = BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(getGlobalContext(),~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());

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
					Value* cmpLhs = BinaryOperator::Create(Instruction::Xor,lhs,ConstantInt::get(getGlobalContext(),~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());

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
			std::cerr << "Unknown affector" << std::endl;
			context.errorFlagged=true;
			return NULL;
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
	if (!context.LookupBitVariable(var,ident.module,ident.name))
	{
		return NULL;
	}
	
	if (var.mappingRef)
	{
		std::cerr << "Cannot perform operation on a mappingRef" << std::endl;
		context.errorFlagged=true;
		return NULL;
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
				answer=ConstantInt::get(getGlobalContext(),bits);
			}
			break;
		case TOK_ZERO:
			answer=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,exprResult, ConstantInt::get(getGlobalContext(),APInt(resultType->getBitWidth(),0,false)), "", context.currentBlock());
			break;
		case TOK_NONZERO:
			answer=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_NE,exprResult, ConstantInt::get(getGlobalContext(),APInt(resultType->getBitWidth(),0,false)), "", context.currentBlock());
			break;
		case TOK_SIGN:
		case TOK_NOSIGN:
			{
				APInt signBit(resultType->getBitWidth(),0,false);
				signBit.setBit(resultType->getBitWidth()-1);
				if (type==TOK_SIGN)
				{
					answer=BinaryOperator::Create(Instruction::And,exprResult,ConstantInt::get(getGlobalContext(),signBit),"",context.currentBlock());
				}
				else
				{
					answer=BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(getGlobalContext(),signBit),"",context.currentBlock());
				}
				answer=BinaryOperator::Create(Instruction::LShr,answer,ConstantInt::get(getGlobalContext(),APInt(resultType->getBitWidth(),(uint64_t)(resultType->getBitWidth()-1),false)),"",context.currentBlock());
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
						Type* ty = Type::getIntNTy(getGlobalContext(),computeClosestPower2Size.getLimitedValue());
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
					Value *shifted = BinaryOperator::Create(Instruction::LShr,answer,ConstantInt::get(getGlobalContext(),computeClosestPower2Size),"",context.currentBlock());
					answer=BinaryOperator::Create(Instruction::Xor,shifted,answer,"",context.currentBlock());
					computeClosestPower2Size=computeClosestPower2Size.lshr(1);
				}

				// final part, mask to nibble size and use this to lookup into magic constant 0x6996 (which is simply a table look up for the parities of a nibble)
				answer=BinaryOperator::Create(Instruction::And,answer,ConstantInt::get(getGlobalContext(),APInt(computeClosestPower2Size.getBitWidth(),0xF,false)),"",context.currentBlock());
				Type* ty = Type::getIntNTy(getGlobalContext(),16);
				Instruction::CastOps op = CastInst::getCastOpcode(answer,false,ty,false);
				answer = CastInst::Create(op,answer,ty,"",context.currentBlock());
				if (type == TOK_PARITYEVEN)
				{
					answer=BinaryOperator::Create(Instruction::LShr,ConstantInt::get(getGlobalContext(),~APInt(16,0x6996,false)),answer,"",context.currentBlock());
				}
				else
				{
					answer=BinaryOperator::Create(Instruction::LShr,ConstantInt::get(getGlobalContext(),APInt(16,0x6996,false)),answer,"",context.currentBlock());
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
					answer=BinaryOperator::Create(Instruction::And,exprResult,ConstantInt::get(getGlobalContext(),bit),"",context.currentBlock());
				}
				else
				{
					answer=BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(getGlobalContext(),bit),"",context.currentBlock());
				}
				answer=BinaryOperator::Create(Instruction::LShr,answer,ConstantInt::get(getGlobalContext(),shift),"",context.currentBlock());
			}
			break;
		case TOK_OVERFLOW:
		case TOK_NOOVERFLOW:
			{
				APInt bit(resultType->getBitWidth(),0,false);
				APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
				bit.setBit(param.integer.getLimitedValue());
				ConstantInt* bitC=ConstantInt::get(getGlobalContext(),bit);
				ConstantInt* shiftC=ConstantInt::get(getGlobalContext(),shift);

				if (param.integer.getLimitedValue() >= resultType->getBitWidth())
				{
					std::cerr << "Bit for overflow detection is outside of range for result" << std::endl;
					context.errorFlagged=true;
					return NULL;
				}
				Value* lhs = ov1Val;		// Lhs and Rhs are provided in the overflow affector
				if (lhs==NULL)
				{
					context.errorFlagged=true;
					return NULL;
				}
				Value* rhs = ov2Val;
				if (rhs==NULL)
				{
					context.errorFlagged=true;
					return NULL;
				}
				
				IntegerType* lhsType = cast<IntegerType>(lhs->getType());
				IntegerType* rhsType = cast<IntegerType>(rhs->getType());
				
				if (param.integer.getLimitedValue() >= lhsType->getBitWidth())
				{
					std::cerr << "Bit for overflow detection is outside of range for source1" << std::endl;
					context.errorFlagged=true;
					return NULL;
				}
				if (param.integer.getLimitedValue() >= rhsType->getBitWidth())
				{
					std::cerr << "Bit for overflow detection is outside of range for source2" << std::endl;
					context.errorFlagged=true;
					return NULL;
				}
		       		
				Instruction::CastOps lhsOp = CastInst::getCastOpcode(lhs,false,resultType,false);
				lhs = CastInst::Create(lhsOp,lhs,resultType,"cast",context.currentBlock());
				Instruction::CastOps rhsOp = CastInst::getCastOpcode(rhs,false,resultType,false);
				rhs = CastInst::Create(rhsOp,rhs,resultType,"cast",context.currentBlock());

				if (opType==TOK_ADD || opType==TOK_DADD)
				{
					//((rh & lh & (~Result)) | ((~rh) & (~lh) & Result))

					// ~Result
					Value* cmpResult = BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(getGlobalContext(),~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());
					// ~lh
					Value* cmplh = BinaryOperator::Create(Instruction::Xor,lhs,ConstantInt::get(getGlobalContext(),~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());
					// ~rh
					Value* cmprh = BinaryOperator::Create(Instruction::Xor,rhs,ConstantInt::get(getGlobalContext(),~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());

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
					Value* cmpResult = BinaryOperator::Create(Instruction::Xor,exprResult,ConstantInt::get(getGlobalContext(),~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());
					// ~lh
					Value* cmplh = BinaryOperator::Create(Instruction::Xor,lhs,ConstantInt::get(getGlobalContext(),~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());
					// ~rh
					Value* cmprh = BinaryOperator::Create(Instruction::Xor,rhs,ConstantInt::get(getGlobalContext(),~APInt(resultType->getBitWidth(),0,false)),"",context.currentBlock());

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
				ConstantInt* bitC=ConstantInt::get(getGlobalContext(),bit);
				ConstantInt* shiftC=ConstantInt::get(getGlobalContext(),shift);

				if (tmpResult==NULL)
				{
					std::cerr<<"Failed to compute CARRY/OVERFLOW for expression (possible bug in compiler)"<<std::endl;
					context.errorFlagged=true;
					return NULL;
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
			std::cerr << "Unknown affector" << std::endl;
			context.errorFlagged=true;
			return NULL;
	}

	return CAssignment::generateAssignment(var,ident,answer,context);
}

void CAffector::prePass(CodeGenContext& context)
{
	expr.prePass(context);
}

Value* CAffector::codeGen(CodeGenContext& context)
{
	Value* left=NULL;
	Value* right=NULL;
	Value* exprResult=NULL;
	bool containsCarryOverflow=false;
	int type=0;

	if (context.curAffectors.size())
	{
		std::cerr<<"Cannot currently supply nested affectors!"<<std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	// If there is more than 1 expr that is used to compute CARRY/OVERFLOW, we need to test at each stage.
	
	for (int a=0;a<affectors.size();a++)
	{
		switch (affectors[a]->type)
		{
			case TOK_CARRY:
			case TOK_NOCARRY:
				containsCarryOverflow=true;
				affectors[a]->tmpResult=NULL;
				context.curAffectors.push_back(affectors[a]);
				break;
			case TOK_OVERFLOW:
			case TOK_NOOVERFLOW:
				containsCarryOverflow=true;
				affectors[a]->tmpResult=NULL;
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
			std::cerr << "OVERFLOW/CARRY is not supported for non carry/overflow expressions" << expr.IsLeaf() << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
	}

	exprResult=expr.codeGen(context);

	// Final result is in exprResult (+affectors for carry/overflow).. do final operations
	for (int a=0;a<affectors.size();a++)
	{
		affectors[a]->codeGenFinal(context,exprResult);
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
			std::cerr << "External C functions must use C size parameters (8,16 or 32 bits!) " << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		FuncTy_8_args.push_back(IntegerType::get(getGlobalContext(), size));
	}
	FunctionType* FuncTy_8;
	if (returns.empty())
	{
		FuncTy_8 = FunctionType::get(Type::getVoidTy(getGlobalContext()),FuncTy_8_args,false);
	}
	else
	{
		unsigned size = returns[0]->integer.getLimitedValue();
		if (size!=8 && size!=16 && size!=32)
		{
			std::cerr << "External C functions must use C size parameters (8,16 or 32 bits!) " << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		FuncTy_8 = FunctionType::get(IntegerType::get(getGlobalContext(), size),FuncTy_8_args,false);
	}

	Function* func = Function::Create(FuncTy_8,GlobalValue::ExternalLinkage,context.symbolPrepend+name.name,context.module);
	func->setCallingConv(CallingConv::C);

	context.m_externFunctions[name.name] = func;

	return NULL;
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
		std::cerr << "Function Declaration Required to use a Function : " << name.name << std::endl;
		context.errorFlagged=true;
		return NULL;
	}
	Function* func = context.m_externFunctions[name.name];

	std::vector<Value*> args;
	const FunctionType* funcType = func->getFunctionType();

	for (int a=0,b=0;a<params.size();a++,b++)
	{
		if (b==funcType->getNumParams())
		{
			std::cerr << "Wrong Number Of Arguments To Function : " << name.name << std::endl;
			context.errorFlagged=true;
			return NULL;
		}

		Value* exprResult = params[a]->codeGen(context);

		// We need to promote the return type to the same as the function parameters (Auto promote rules)
		Instruction::CastOps op = CastInst::getCastOpcode(exprResult,false,funcType->getParamType(b),false);
		exprResult = CastInst::Create(op,exprResult,funcType->getParamType(b),"",context.currentBlock());

		args.push_back(exprResult);
	}
	Value* call;
	if (funcType->getReturnType()->isVoidTy())
	{
		call = CallInst::Create(func, args, "", context.currentBlock());

		call = ConstantInt::get(getGlobalContext(),APInt(1,0));		// Forces void returns to actually return 0
	}
	else
	{
		call = CallInst::Create(func, args, "CallingCFunc"+name.name, context.currentBlock());
	}

	return call;
}

CInteger CVariableDeclaration::notArray(stringZero);
CInteger CTrigger::zero(stringZero);

Value* CTrigger::codeGen(CodeGenContext& context,BitVariable& pin,Value* function)
{
	switch (type)
	{
		case TOK_ALWAYS:
			{
				CallInst* fcall = CallInst::Create(function,"",*pin.writeAccessor);
			}
			return NULL;
		case TOK_CHANGED:
			// Compare input value to old value, if different then call function 
			{
				BasicBlock *oldBlock = (*pin.writeAccessor)->getParent();
				Function* parentFunction = oldBlock->getParent();

				context.pushBlock(oldBlock);
				Value* prior = CIdentifier::GetAliasedData(context,pin.priorValue,pin);
				Value* writeInput = CIdentifier::GetAliasedData(context,pin.writeInput,pin);
				Value* answer=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,prior,writeInput, "", context.currentBlock());
				
				BasicBlock *ifcall = BasicBlock::Create(getGlobalContext(),"ifcall",parentFunction);
				BasicBlock *nocall = BasicBlock::Create(getGlobalContext(),"nocall",parentFunction);

				BranchInst::Create(nocall,ifcall,answer,context.currentBlock());
				context.popBlock();
				
				CallInst* fcall = CallInst::Create(function,"",ifcall);
				BranchInst::Create(nocall,ifcall);

				// Remove return instruction (since we need to create a new basic block set
				(*pin.writeAccessor)->removeFromParent();

				*pin.writeAccessor=ReturnInst::Create(getGlobalContext(),nocall);

			}
			return NULL;
		case TOK_TRANSITION:
			// Compare input value to param2 and old value to param1 , if same call function 
			{
				BasicBlock *oldBlock = (*pin.writeAccessor)->getParent();
				Function* parentFunction = oldBlock->getParent();

				context.pushBlock(oldBlock);
				Value* prior = CIdentifier::GetAliasedData(context,pin.priorValue,pin);
				Value* writeInput = CIdentifier::GetAliasedData(context,pin.writeInput,pin);

				Value* checkParam2=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,ConstantInt::get(getGlobalContext(),param2.integer.zextOrTrunc(pin.trueSize.getLimitedValue())),writeInput, "", context.currentBlock());
				Value* checkParam1=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,ConstantInt::get(getGlobalContext(),param1.integer.zextOrTrunc(pin.trueSize.getLimitedValue())),prior, "", context.currentBlock());
		
				Value *answer=BinaryOperator::Create(Instruction::And,checkParam1,checkParam2,"",context.currentBlock());
				
				BasicBlock *ifcall = BasicBlock::Create(getGlobalContext(),"ifcall",parentFunction);
				BasicBlock *nocall = BasicBlock::Create(getGlobalContext(),"nocall",parentFunction);

				BranchInst::Create(ifcall,nocall,answer,context.currentBlock());
				context.popBlock();
				
				CallInst* fcall = CallInst::Create(function,"",ifcall);
				BranchInst::Create(nocall,ifcall);

				// Remove return instruction (since we need to create a new basic block set
				(*pin.writeAccessor)->removeFromParent();

				*pin.writeAccessor=ReturnInst::Create(getGlobalContext(),nocall);

			}
			return NULL;

		default:
			std::cerr<< "Unhandled trigger type" << std::endl;
			context.errorFlagged=true;
			return NULL;
	}

	return NULL;
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
		FuncTy_8_args.push_back(IntegerType::get(getGlobalContext(), size));
	}
	FunctionType* FuncTy_8;

	if (returns.empty())
	{
		FuncTy_8 = FunctionType::get(Type::getVoidTy(getGlobalContext()),FuncTy_8_args,false);
	}
	else
	{
		unsigned size = returns[0]->size.integer.getLimitedValue();
		FuncTy_8 = FunctionType::get(IntegerType::get(getGlobalContext(), size),FuncTy_8_args,false);
	}

	Function* func = NULL;
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
	
	context.m_externFunctions[name.name] = func;

	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", func, 0);
	
	context.pushBlock(bblock);

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
		returnVal.writeAccessor=NULL;
		returnVal.writeInput=NULL;
		returnVal.priorValue=NULL;
		returnVal.impedance=NULL;

		AllocaInst *alloc = new AllocaInst(Type::getIntNTy(getGlobalContext(),returns[0]->size.integer.getLimitedValue()), returns[0]->id.name.c_str(), context.currentBlock());
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
		temp.mask = ~returnVal.cnst;
		temp.shft = APInt(params[a]->size.integer.getLimitedValue(),0);
		temp.aliased = false;
		temp.mappingRef = false;
		temp.pinType=0;
		temp.writeAccessor=NULL;
		temp.writeInput=NULL;
		temp.priorValue=NULL;
		temp.impedance=NULL;

		temp.value=args;
		temp.value->setName(params[a]->id.name);
		context.locals()[params[a]->id.name]=temp;
		a++;
		args++;
	}

	block.codeGen(context);

	if (returns.empty())
	{
		ReturnInst::Create(getGlobalContext(), context.currentBlock());			/* block may well have changed by time we reach here */
	}
	else
	{
		ReturnInst::Create(getGlobalContext(),new LoadInst(returnVal.value,"",context.currentBlock()),context.currentBlock());
	}

	context.popBlock();

	return func;
}

extern int yyparse();
extern CBlock* g_ProgramBlock;
extern FILE *yyin;

#include <stdio.h>

void CInstance::prePass(CodeGenContext& context)
{
	// On the prepass, we need to generate the code for this module
	std::string includeName = filename.quoted.substr(1,filename.quoted.length()-2);

	yyin = fopen(includeName.c_str(),"r");
	if (!yyin)
	{
		std::cerr << "Unable to instance module : " << includeName << endl;
		context.errorFlagged=true;
		return;
	}

	g_ProgramBlock=0;
	yyparse();
	if (g_ProgramBlock==0)
	{
		std::cerr << "Error : Unable to parse module : " << includeName << endl;
		context.errorFlagged=true;
		return;
	}
	
	CodeGenContext* includefile;

	CompilerOptions dummy;

	includefile = new CodeGenContext(&context);
	includefile->generateCode(*g_ProgramBlock,dummy);

	context.m_includes[ident.name]=includefile;
}

Value* CInstance::codeGen(CodeGenContext& context)
{
	// At present doesn't generate any code - maybe never will
	return NULL;
}

Value* CExchange::codeGen(CodeGenContext& context)
{
	BitVariable lhsVar,rhsVar;

	if (!context.LookupBitVariable(lhsVar,lhs.module,lhs.name))
	{
		return NULL;
	}
	if (!context.LookupBitVariable(rhsVar,rhs.module,rhs.name))
	{
		return NULL;
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
				std::cerr << "Both operands to exchange must be same size!" << std::endl;
				context.errorFlagged=true;
				return NULL;
			}
		
			CAssignment::generateAssignment(lhsVar,lhs,right,context);
			CAssignment::generateAssignment(rhsVar,rhs,left,context);
		}
		
		return NULL;
	}
	
	std::cerr<<"Illegal operands to exchange"<<std::endl;
	context.errorFlagged=true;
	return NULL;
}

