#include "ast.h"
#include "generator.h"
#include "parser.hpp"

using namespace std;

/* Compile the AST into a module */
void CodeGenContext::generateCode(CBlock& root)
{
	std::cout << "Generating code...\n";
	
	ee = EngineBuilder(module).create();

	PointerType* PointerTy_4 = PointerType::get(IntegerType::get(getGlobalContext(), 8), 0);

  	std::vector<const Type*>FuncTy_8_args;
  	FuncTy_8_args.push_back(PointerTy_4);
  	FunctionType* FuncTy_8 = FunctionType::get(/*Result=*/IntegerType::get(getGlobalContext(), 32),/*Params=*/FuncTy_8_args,/*isVarArg=*/false);

  	debugTraceString = Function::Create(/*Type=*/FuncTy_8,/*Linkage=*/GlobalValue::ExternalLinkage,/*Name=*/"puts", module); // (external, no body)
	debugTraceString->setCallingConv(CallingConv::C);
	AttrListPtr func_puts_PAL;
	debugTraceString->setAttributes(func_puts_PAL);

	std::vector<const Type*>FuncTy_6_args;
	FuncTy_6_args.push_back(IntegerType::get(getGlobalContext(), 32));
	FunctionType* FuncTy_6 = FunctionType::get(/*Result=*/IntegerType::get(getGlobalContext(), 32),/*Params=*/FuncTy_6_args,/*isVarArg=*/false);

  	debugTraceChar = Function::Create(/*Type=*/FuncTy_6,/*Linkage=*/GlobalValue::ExternalLinkage,/*Name=*/"putchar", module); // (external, no body)
	debugTraceChar->setCallingConv(CallingConv::C);
	debugTraceChar->setAttributes(func_puts_PAL);
	
	root.codeGen(*this);	/* Generate complete code - starting at no block (global space) */

	/* Finally create an entry point - which does nothing for now */
	vector<const Type*> argTypes;
	FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext())/* Type::getInt64Ty(getGlobalContext())*/, argTypes, false);
	mainFunction = Function::Create(ftype, GlobalValue::ExternalLinkage, "main", module);
	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", mainFunction, 0);
	
	/* Push a new variable/block context */
	pushBlock(bblock);

	for (int a=0;a<9;a++)
	{
		for (int b=0;b<handlersToTest.size();b++)
		{
			CallInst::Create(handlersToTest[b],"",bblock);
		}
	}

	ReturnInst::Create(getGlobalContext(), bblock);
	popBlock();
	
	/* Print the bytecode in a human-readable format 
	   to see if our program compiled properly
	 */
	std::cout << "Code is generated.\n";
	PassManager pm;
/*	pm.add(new TargetData(*ee->getTargetData()));
	pm.add(createBasicAliasAnalysisPass());
	pm.add(createInstructionCombiningPass());
	pm.add(createReassociatePass());
	pm.add(createGVNPass());
	pm.add(createCFGSimplificationPass());
*/
	pm.add(createPrintModulePass(&outs()));
    pm.add(createVerifierPass());

    // Add an appropriate TargetData instance for this module...
    pm.add(new TargetData(*ee->getTargetData()));
    
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

    // Make sure everything is still good.
    pm.add(createVerifierPass());

	pm.add(createPrintModulePass(&outs()));

	pm.run(*module);
}

/* Executes the AST by running the main function */
GenericValue CodeGenContext::runCode() {
	std::cout << "Running code...\n";
	vector<GenericValue> noargs;
	GenericValue v = ee->runFunction(mainFunction, noargs);
	std::cout << "Code was run. returned :" << v.IntVal.getZExtValue() << endl;
	return v;
}

/* -- Code Generation -- */

Value* CInteger::codeGen(CodeGenContext& context)
{
	std::cout << "Creating integer: " << integer.toString(10,false) << endl;
	return ConstantInt::get(getGlobalContext(), integer);
}

Value* CString::codeGen(CodeGenContext& context)
{
	std::cout << "Creating string : " << quoted << endl;

	ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(getGlobalContext(), 8), quoted.length()-1);
 
       	GlobalVariable* gvar_array__str = new GlobalVariable(/*Module=*/*context.module,  /*Type=*/ArrayTy_0,  /*isConstant=*/true,  /*Linkage=*/GlobalValue::PrivateLinkage,  /*Initializer=*/0,  /*Name=*/".str");
	gvar_array__str->setAlignment(1);
  
	// Constant Definitions
	Constant* const_array_9 = ConstantArray::get(getGlobalContext(), quoted.substr(1,quoted.length()-2), true);
	std::vector<Constant*> const_ptr_12_indices;
	ConstantInt* const_int64_13 = ConstantInt::get(getGlobalContext(), APInt(64, StringRef("0"), 10));
	const_ptr_12_indices.push_back(const_int64_13);
	const_ptr_12_indices.push_back(const_int64_13);
	Constant* const_ptr_12 = ConstantExpr::getGetElementPtr(gvar_array__str, &const_ptr_12_indices[0], const_ptr_12_indices.size());

	// Global Variable Definitions
	gvar_array__str->setInitializer(const_array_9);

	return const_ptr_12;
}

Value* CIdentifier::trueSize(Value* in,CodeGenContext& context)
{
	BitVariable var;
	if (context.locals().find(name) == context.locals().end())
	{
		if (context.globals().find(name) == context.globals().end())
		{
			std::cerr << "undeclared variable " << name << endl;
			return NULL;
		}
		else
		{
			var=context.globals()[name];
		}
	}
	else
	{
		var=context.locals()[name];
	}
	
	const Type* ty = Type::getIntNTy(getGlobalContext(),var.trueSize.getLimitedValue());
	Instruction::CastOps op = CastInst::getCastOpcode(in,false,ty,false);

	Instruction* truncExt = CastInst::Create(op,in,ty,"cast",context.currentBlock());

	return truncExt;
}

Value* CIdentifier::codeGen(CodeGenContext& context)
{
	BitVariable var;
	std::cout << "Creating identifier reference: " << name << endl;
	if (context.locals().find(name) == context.locals().end())
	{
		if (context.globals().find(name) == context.globals().end())
		{
			std::cerr << "undeclared variable " << name << endl;
			return NULL;
		}
		else
		{
			var=context.globals()[name];
		}
	}
	else
	{
		var=context.locals()[name];
	}

	Value* final = new LoadInst(var.value, "", false, context.currentBlock());

	if (var.aliased == true)
	{
		// We are loading from a partial value - we need to load, mask off correct result and shift result down to correct range
		ConstantInt* const_intMask = ConstantInt::get(getGlobalContext(), var.mask);	
		BinaryOperator* andInst = BinaryOperator::Create(Instruction::And, final, const_intMask , "Masking", context.currentBlock());
		ConstantInt* const_intShift = ConstantInt::get(getGlobalContext(), var.shft);
		BinaryOperator* shiftInst = BinaryOperator::Create(Instruction::LShr,andInst ,const_intShift, "Shifting", context.currentBlock());		// Need to shrink aliased type to correct size! at present this is done by calling trueSize - since it generates less code under normal circumstances, and i only need true size when displaying via DEBUG_TRACE
		final = shiftInst;
	}

	return final;
}

CIdentifier CAliasDeclaration::empty("");

Value* CAliasDeclaration::codeGen(CodeGenContext& context)
{
	std::cout << "Create alias declaration - Doing nothing" << endl;
	return NULL;
}

Value* CMethodCall::codeGen(CodeGenContext& context)
{
	Function *function = context.module->getFunction(id.name.c_str());
	if (function == NULL) {
		std::cerr << "no such function " << id.name << endl;
	}
	std::vector<Value*> args;
	ExpressionList::const_iterator it;
	for (it = arguments.begin(); it != arguments.end(); it++) {
		args.push_back((**it).codeGen(context));
	}
	CallInst *call = CallInst::Create(function, args.begin(), args.end(), "", context.currentBlock());
	std::cout << "Creating method call: " << id.name << endl;
	return call;
}

Value* CDebugTraceString::codeGen(CodeGenContext& context)
{
/*	std::vector<Value*> args;
	args.push_back(string.codeGen(context));
	CallInst *call = CallInst::Create(context.debugTraceString,args.begin(),args.end(),"DEBUGTRACE",context.currentBlock());
	return call;*/
	for (unsigned a=1;a<string.quoted.length()-1;a++)
	{
		std::vector<Value*> args;
		
		args.push_back(ConstantInt::get(getGlobalContext(),APInt(32,string.quoted[a])));
		CallInst *call = CallInst::Create(context.debugTraceChar,args.begin(),args.end(),"DEBUGTRACE",context.currentBlock());
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

	for (unsigned a=0;a<cnt;a++)
	{
		std::vector<Value*> args;

		APInt tmp = integer.integer.udiv(baseDivisor);

		args.push_back(ConstantInt::get(getGlobalContext(), APInt(32, tbl[tmp.getLimitedValue()])));
		CallInst *call = CallInst::Create(context.debugTraceChar,args.begin(),args.end(),"DEBUGTRACE",context.currentBlock());

		integer.integer-=tmp*baseDivisor;
		baseDivisor=baseDivisor.udiv(APInt(bitWidth,currentBase));
	}
	return fcall;
}

Value* CDebugTraceIdentifier::codeGen(CodeGenContext& context)
{
	CallInst* fcall;

	std::string tmp = "\"";
	tmp+=ident.name;
	tmp+="(\"";
	CString strng(tmp);
	CDebugTraceString identName(strng);

	identName.codeGen(context);

	Value* loadedValue = ident.trueSize(ident.codeGen(context),context);
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

	std::cout << "currentBase " << currentBase << std::endl;

	baseDivisor=oldBaseDivisor.trunc(bitWidth);
	if (baseDivisor.getLimitedValue()==0)
	{
		baseDivisor = APInt(bitWidth,1);
	}

	for (unsigned a=0;a<cnt;a++)
	{
		std::vector<Value*> args;
		
		std::cout << " SHIT BEANS : " << cnt << " : " << baseDivisor.toString(10,false) << std::endl;

		ConstantInt* const_intDiv = ConstantInt::get(getGlobalContext(), baseDivisor);
		BinaryOperator* shiftInst = BinaryOperator::Create(Instruction::UDiv,loadedValue,const_intDiv, "Dividing", context.currentBlock());
		
		const Type* ty = Type::getIntNTy(getGlobalContext(),32);
		Instruction::CastOps op = CastInst::getCastOpcode(shiftInst,false,ty,false);
		Instruction* readyToAdd = CastInst::Create(op,shiftInst,ty,"cast",context.currentBlock());
		
		CmpInst* check=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_ULE,readyToAdd,ConstantInt::get(getGlobalContext(),APInt(32,9)),"rangeCheck",context.currentBlock());

		BinaryOperator* lowAdd = BinaryOperator::Create(Instruction::Add,readyToAdd,ConstantInt::get(getGlobalContext(),APInt(32,'0')),"lowAdd",context.currentBlock());
		BinaryOperator* hiAdd = BinaryOperator::Create(Instruction::Add,readyToAdd,ConstantInt::get(getGlobalContext(),APInt(32,'A'-10)),"lowAdd",context.currentBlock());

		SelectInst *select = SelectInst::Create(check,lowAdd,hiAdd,"getRightChar",context.currentBlock());

		args.push_back(select);
		CallInst *call = CallInst::Create(context.debugTraceChar,args.begin(),args.end(),"DEBUGTRACE",context.currentBlock());
		
		BinaryOperator* mulUp = BinaryOperator::Create(Instruction::Mul,shiftInst,const_intDiv,"mulUp",context.currentBlock());
		loadedValue = BinaryOperator::Create(Instruction::Sub,loadedValue,mulUp,"fixup",context.currentBlock());
		if (cnt!=1)
		{
			baseDivisor=baseDivisor.udiv(APInt(bitWidth,currentBase));
		}
	}
	{
		std::vector<Value*> args;
		args.push_back(ConstantInt::get(getGlobalContext(), APInt(32, ')')));
		fcall = CallInst::Create(context.debugTraceChar,args.begin(),args.end(),"DEBUGTRACE",context.currentBlock());
	}
	return fcall;
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
	CallInst* fcall = CallInst::Create(context.debugTraceChar,args.begin(),args.end(),"DEBUGTRACE",context.currentBlock());
	
	BranchInst::Create(cont,context.currentBlock());

	context.popBlock();

	context.setBlock(cont);

	return NULL;
}

Value* CBinaryOperator::codeGen(CodeGenContext& context)
{
	Value* left;
	Value* right;

	left = lhs.codeGen(context);
	right = rhs.codeGen(context);

	if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
	{
		const IntegerType* leftType = cast<IntegerType>(left->getType());
		const IntegerType* rightType = cast<IntegerType>(right->getType());

		if (leftType->getBitWidth() < rightType->getBitWidth())
		{
			Instruction::CastOps op = CastInst::getCastOpcode(left,false,rightType,false);

			left = CastInst::Create(op,left,rightType,"cast",context.currentBlock());
		}
		if (leftType->getBitWidth() > rightType->getBitWidth())
		{
			Instruction::CastOps op = CastInst::getCastOpcode(right,false,leftType,false);

			right = CastInst::Create(op,right,leftType,"cast",context.currentBlock());
		}

		std::cout << "Creating binary operation " << op << endl;
		switch (op) 
		{
		case TOK_ADD:
			return BinaryOperator::Create(Instruction::Add,left,right,"",context.currentBlock());
		case TOK_SUB:
			return BinaryOperator::Create(Instruction::Sub,left,right,"",context.currentBlock());
		case TOK_CMPEQ:
			return CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,left, right, "", context.currentBlock());
		case TOK_DBAR:
			return BinaryOperator::Create(Instruction::Or,left,right,"",context.currentBlock());
		case TOK_DAMP:
			return BinaryOperator::Create(Instruction::And,left,right,"",context.currentBlock());
		}
	}


	return NULL;
}

Value* CAssignment::codeGen(CodeGenContext& context)
{
	BitVariable var;
	Value* assignTo;
	Value* assignWith;
	std::cout << "Creating assignment for " << lhs.name << endl;
	if (context.locals().find(lhs.name) == context.locals().end())
	{
		if (context.globals().find(lhs.name) == context.globals().end())
		{
			std::cerr << "undeclared variable " << lhs.name << endl;
			return NULL;
		}
		else
		{
			var=context.globals()[lhs.name];
		}
	}
	else
	{
		var=context.locals()[lhs.name];
	}

	assignTo = var.value;
	assignWith = rhs.codeGen(context);

	// Handle variable promotion
	const Type* ty = Type::getIntNTy(getGlobalContext(),var.size.getLimitedValue());
	Instruction::CastOps op = CastInst::getCastOpcode(assignWith,false,ty,false);

	Instruction* truncExt = CastInst::Create(op,assignWith,ty,"cast",context.currentBlock());

	// Handle masking and constants and shift
	ConstantInt* const_intShift = ConstantInt::get(getGlobalContext(), var.shft);
	BinaryOperator* shiftInst = BinaryOperator::Create(Instruction::Shl,truncExt,const_intShift, "Shifting", context.currentBlock());
	ConstantInt* const_intMask = ConstantInt::get(getGlobalContext(), var.mask);	
	BinaryOperator* andInst = BinaryOperator::Create(Instruction::And, shiftInst, const_intMask , "Masking", context.currentBlock());

	// cnst initialiser only used when we are updating the primary register
	BinaryOperator* final;
	if (var.aliased == false)
	{
		ConstantInt* const_intCnst = ConstantInt::get(getGlobalContext(), var.cnst);
		BinaryOperator* orInst = BinaryOperator::Create(Instruction::Or, andInst, const_intCnst, "Constants", context.currentBlock());
		final=orInst;
	}
	else
	{
		// Now if the assignment is assigning to an aliased register part, we need to have loaded the original register, masked off the inverse of the section mask, and or'd in the result before we store
		LoadInst* loadInst=new LoadInst(var.value, "", false, context.currentBlock());
		ConstantInt* const_intInvMask = ConstantInt::get(getGlobalContext(), ~var.mask);
		BinaryOperator* primaryAndInst = BinaryOperator::Create(Instruction::And, loadInst, const_intInvMask , "InvMasking", context.currentBlock());
		final = BinaryOperator::Create(Instruction::Or,primaryAndInst,andInst,"Combining",context.currentBlock());
	}

	return new StoreInst(final, assignTo, false, context.currentBlock());
}

Value* CBlock::codeGen(CodeGenContext& context)
{
	StatementList::const_iterator it;
	Value *last = NULL;
	for (it = statements.begin(); it != statements.end(); it++) {
		std::cout << "Generating code for " << typeid(**it).name() << endl;
		last = (**it).codeGen(context);
	}
	std::cout << "Creating block" << endl;
	return last;
}

Value* CExpressionStatement::codeGen(CodeGenContext& context)
{
	std::cout << "Generating code for " << typeid(expression).name() << endl;
	return expression.codeGen(context);
}

Value* CStateDeclaration::codeGen(CodeGenContext& context,Function* parent)
{
	std::cout << "Creating state enumeration " << id.name << endl;
		
	// We need to create a bunch of labels - one for each case.. we will need a finaliser for the blocks when we pop out of our current block.
	entry = BasicBlock::Create(getGlobalContext(),id.name+"entry",parent);
	exit = BasicBlock::Create(getGlobalContext(),id.name+"exit",parent);

	return NULL;
}

Value* CStatesDeclaration::codeGen(CodeGenContext& context)
{
	Twine numStatesTwine(states.size());
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();

	std::cout << "Generating state machine entry " << endl;

	std::cout << "Number of states : " << numStates << " " << bitsNeeded << endl;

	std::string curStateLbl = "CUR" + context.stateLabelStack;
	std::string nxtStateLbl = "NEXT" + context.stateLabelStack;
	std::string stateLabel = "STATE" + context.stateLabelStack;

	GlobalVariable *curState = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),bitsNeeded), false, GlobalValue::InternalLinkage,NULL,curStateLbl);
	GlobalVariable *nxtState = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),bitsNeeded), false, GlobalValue::InternalLinkage,NULL,nxtStateLbl);
 
	StateVariable newStateVar;
	newStateVar.currentState = curState;
	newStateVar.nextState = nxtState;
	newStateVar.decl = this;

	context.states()[stateLabel]=newStateVar;

	// Constant Definitions
	ConstantInt* const_int32_n = ConstantInt::get(getGlobalContext(), APInt(bitsNeeded, 0, false));
  
	// Global Variable Definitions
	curState->setInitializer(const_int32_n);
	nxtState->setInitializer(const_int32_n);

	BasicBlock* bb = context.currentBlock();		// cache old basic block
	std::map<std::string,BitVariable> tmp = context.locals();

	exitState = BasicBlock::Create(getGlobalContext(),"switchTerm",bb->getParent());
	context.setBlock(exitState);

	// Step 1, load next state into current state
	LoadInst* getNextState = new LoadInst(nxtState,"",false,bb);
	StoreInst* storeState = new StoreInst(getNextState,curState,false,bb);

	// Step 2, generate switch statement
	LoadInst* int32_5 = new LoadInst(curState, "", false, bb);
	SwitchInst* void_6 = SwitchInst::Create(int32_5, exitState,states.size(), bb);

	// Step 3, build labels and initialiser for next state on entry
	bool lastStateAutoIncrement=false;
	int startOfAutoIncrementIdx=-1;
	for (int a=0;a<states.size();a++)
	{
		std::cout << "WEEE" << a << std::endl;
		// Build Labels
		states[a]->codeGen(context,bb->getParent());
		ConstantInt* tt = ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,a,false));
		void_6->addCase(tt,states[a]->entry);

		// Compute next state and store for future
		if (states[a]->autoIncrement)
		{
			if (!lastStateAutoIncrement)
			{
				startOfAutoIncrementIdx=a;
			}
			ConstantInt* nextState = ConstantInt::get(getGlobalContext(),APInt(bitsNeeded, a==states.size()-1 ? 0 : a+1,false));
			StoreInst* newState = new StoreInst(nextState,nxtState,false,states[a]->entry);
		}
		else
		{
			if (lastStateAutoIncrement)
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
		BranchInst::Create(exitState,states[a]->exit);			// this terminates the final blocks from our states
	}

	return NULL;
}

Value* CVariableDeclaration::codeGen(CodeGenContext& context)
{
	BitVariable temp;

	std::cout << "Creating variable declaration " << id.name << " " << size.integer.toString(10,false) << endl;

	temp.size = size.integer;
	temp.trueSize = size.integer;
	temp.cnst = APInt(size.integer.getLimitedValue(),0);
	temp.mask = ~temp.cnst;
	temp.shft = APInt(size.integer.getLimitedValue(),0);
	temp.aliased = false;

	if (context.currentBlock())
	{
		// Within a basic block - so must be a stack variable
		AllocaInst *alloc = new AllocaInst(Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()), id.name.c_str(), context.currentBlock());
		temp.value = alloc;
	}
	else
	{
		// GLobal variable
		GlobalVariable *global = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()), false, GlobalValue::ExternalLinkage,NULL,id.name.c_str());
		temp.value = global;
	}

	APInt bitPos = size.integer -1;
	for (int a=0;a<aliases.size();a++)
	{
		if (aliases[a]->idOrEmpty.name.size()<1)
		{
			// For constants we need to update the mask and cnst values (we also need to set the initialiser properly - that will come later)

			APInt newMask = ~APInt(aliases[a]->sizeOrValue.integer.getBitWidth(),0);
			APInt newCnst = aliases[a]->sizeOrValue.integer;

			bitPos-=APInt(32,aliases[a]->sizeOrValue.integer.getBitWidth()-1);

	    		newMask=newMask.zext(size.integer.getLimitedValue());
	    		newCnst=newCnst.zext(size.integer.getLimitedValue());
	    		newCnst<<=bitPos.getLimitedValue();
	    		newMask<<=bitPos.getLimitedValue();
	    		temp.mask&=~newMask;
	    		temp.cnst|=newCnst;

			bitPos-=APInt(32,1);
		}
		else
		{
			// For identifiers, we need to create another entry in the correct symbol table and set up the masks appropriately
			// e.g. BOB[4] ALIAS CAT[2]:%01
			// would create CAT with a shift of 2 a mask of %1100 (cnst will always be 0 for these)

			bitPos-=aliases[a]->sizeOrValue.integer-1;

			BitVariable alias;
			alias.size = temp.size;
			alias.trueSize = aliases[a]->sizeOrValue.integer;
			alias.value = temp.value;	// the value will always point at the stored local/global
			alias.cnst = temp.cnst;		// ignored

			APInt newMask = ~APInt(aliases[a]->sizeOrValue.integer.getLimitedValue(),0);
	    		newMask=newMask.zext(size.integer.getLimitedValue());
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

			bitPos-=APInt(32,1);
		}
	}

	ConstantInt* const_intn_0 = ConstantInt::get(getGlobalContext(), temp.cnst);
	if (context.currentBlock())
	{
		// Initialiser Definitions
		StoreInst* stor = new StoreInst(const_intn_0,temp.value,false,context.currentBlock());		// NOT SURE HOW WELL THIS WILL WORK IN FUTURE
		
		context.locals()[id.name] = temp;
	}
	else
	{
		// Initialiser Definitions
		cast<GlobalVariable>(temp.value)->setInitializer(const_intn_0);

		context.globals()[id.name]=temp;
	}

	return temp.value;
}

Value* CStateDefinition::codeGen(CodeGenContext& context)
{
	CStatesDeclaration* pStates = context.currentState();
	if (pStates)
	{
		CStateDeclaration* pState = pStates->getStateDeclaration(id);

	       	if (pState)
		{
			std::string oldStateLabelStack = context.stateLabelStack;
			context.stateLabelStack+="." + id.name;
	
			std::cout << "STATE LABEL : " << context.stateLabelStack << std::endl;

			context.pushBlock(pState->entry);
			block.codeGen(context);
			BranchInst::Create(pState->exit,context.currentBlock());
			context.popBlock();

			context.stateLabelStack=oldStateLabelStack;
		}
		else
		{
			std::cerr << "Attempt to define unknown state : " << id.name.c_str() << endl;
			return NULL;
		}
	}
	else
	{
		std::cerr << "Attempt to define state entry when no states on stack" << endl;
		return NULL;
	}
}

Value* CHandlerDeclaration::codeGen(CodeGenContext& context)
{
	vector<const Type*> argTypes;
	FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext()),argTypes, false);
	Function* function = Function::Create(ftype, GlobalValue::ExternalLinkage, id.name.c_str(), context.module);
	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", function, 0);

	if (id.name == "O1" || id.name == "O2")
	{
		context.handlersToTest.push_back(function);
	}

	context.pushBlock(bblock);

	std::string oldStateLabelStack = context.stateLabelStack;
	context.stateLabelStack+="." + id.name;

	std::cout << "STATE LABEL : " << context.stateLabelStack << std::endl;

	block.codeGen(context);

	ReturnInst::Create(getGlobalContext(), context.currentBlock());			/* block may well have changed by time we reach here */

	context.stateLabelStack=oldStateLabelStack;

	context.popBlock();

	std::cout << "Creating function: " << id.name << endl;

	return function;
}

// Language decision : TEST.A.B@ should only return true if we are in STATE A SUBSTATE B - To do this we need to check each of the states, not just the last one

Value* CStateTest::codeGen(CodeGenContext& context)
{
	unsigned int a,b;
	unsigned int numStatesToCheck = stateIdents.size()-1;
	Value *res=NULL;

	// Locate the parent variable and value for the test
	for (b=0;b<numStatesToCheck;b++)
	{
		std::string findString = "STATE";
		for (a=0;a<b+1;a++)
		{
			findString+="." + stateIdents[a]->name;
		}

		if (context.states().find(findString) == context.states().end())
		{
			std::cout << "Unable to find HANDLER/SUBSTATE " << std::endl;
			return NULL;
		}
	
		StateVariable state = context.states()[findString];

		int stateIndex = state.decl->getStateDeclarationIndex(stateIdents[b+1]->name);
		if (stateIndex == -1)
		{
			std::cout << "Unable to find value for state " << std::endl;
			return NULL;
		}

		Twine numStatesTwine(state.decl->states.size());
		std::string numStates = numStatesTwine.str();
		APInt overSized(4*numStates.length(),numStates,10);
		unsigned bitsNeeded = overSized.getActiveBits();
	
		// Load value from state variable, test against being equal to found index, jump on result
		Value* load = new LoadInst(state.currentState, "", false, context.currentBlock());
		CmpInst* cmp = CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,load, ConstantInt::get(getGlobalContext(), APInt(bitsNeeded,stateIndex)), "", context.currentBlock());

		if (b!=0)
		{
			res = BinaryOperator::Create(Instruction::And,res,cmp,"Combining",context.currentBlock());
		}
		else
		{
			res = cmp;
		}
	}

	return res;
}

Value* CStateJump::codeGen(CodeGenContext& context)
{
	unsigned int a;

	// Locate the parent variable and value for the test
	std::string findString = "STATE";
	for (a=0;a<stateIdents.size()-1;a++)
	{
		findString+="." + stateIdents[a]->name;
	}

	if (context.states().find(findString) == context.states().end())
	{
		std::cout << "Unable to find HANDLER/SUBSTATE " << std::endl;
		return NULL;
	}
	
	StateVariable state = context.states()[findString];

	int stateIndex = state.decl->getStateDeclarationIndex(stateIdents[stateIdents.size()-1]->name);
	if (stateIndex == -1)
	{
		std::cout << "Unable to find value for state " << std::endl;
		return NULL;
	}

	Twine numStatesTwine(state.decl->states.size());
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();

	// We need to store new state into global, but there is a bit of fixup required if we are within a state block (since we don't want to autoincrement our state in that case)
	Value* stor = new StoreInst(ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,stateIndex)),state.nextState,false,context.currentBlock());

	return stor;
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
