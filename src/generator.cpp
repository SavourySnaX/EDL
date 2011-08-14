#include "ast.h"
#include "generator.h"
#include "parser.hpp"

using namespace std;

extern bool JustCompiledOutput;

#define USE_OPTIMISER	1

/* Compile the AST into a module */
void CodeGenContext::generateCode(CBlock& root)
{
	errorFlagged = false;

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

	if (!JustCompiledOutput)
	{
		/* Finally create an entry point - which does nothing for now */
		vector<const Type*> argTypes;
		FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext())/* Type::getInt64Ty(getGlobalContext())*/, argTypes, false);
		mainFunction = Function::Create(ftype, GlobalValue::ExternalLinkage, "moduleTester", module);
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
	}

	if (errorFlagged)
	{
		std::cerr << "Compilation Failed" << std::endl;
		return;
	}

	/* Print the bytecode in a human-readable format 
	   to see if our program compiled properly
	 */
	PassManager pm;

	if (!JustCompiledOutput)
	{
		pm.add(createPrintModulePass(&outs()));
	}
	pm.add(createVerifierPass());

#if USE_OPTIMISER
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
#endif
	pm.run(*module);
}

/* Executes the AST by running the main function */
GenericValue CodeGenContext::runCode() 
{
	GenericValue v;

	if (!errorFlagged && !JustCompiledOutput)
	{
		std::cout << "Running code...\n";
		vector<GenericValue> noargs;
		v = ee->runFunction(mainFunction, noargs);
	}
	return v;
}

/* -- Code Generation -- */

Value* CInteger::codeGen(CodeGenContext& context)
{
	return ConstantInt::get(getGlobalContext(), integer);
}

Value* CString::codeGen(CodeGenContext& context)
{
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
			context.errorFlagged=true;
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
	
	if (var.mappingRef)
	{
		std::cerr << "Cannot perform operation on a mapping Ref!" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	const Type* ty = Type::getIntNTy(getGlobalContext(),var.trueSize.getLimitedValue());
	Instruction::CastOps op = CastInst::getCastOpcode(in,false,ty,false);

	Instruction* truncExt = CastInst::Create(op,in,ty,"cast",context.currentBlock());

	return truncExt;
}

Value* CIdentifier::codeGen(CodeGenContext& context)
{
	BitVariable var;
	if (context.locals().find(name) == context.locals().end())
	{
		if (context.globals().find(name) == context.globals().end())
		{
			std::cerr << "undeclared variable " << name << endl;
			context.errorFlagged=true;
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

	if (var.mappingRef)
	{
		return var.value;
	}

	Value* final = new LoadInst(var.value, "", false, context.currentBlock());

	if (var.aliased == true)
	{
		// We are loading from a partial value - we need to load, mask off correct result and shift result down to correct range
		ConstantInt* const_intMask = ConstantInt::get(getGlobalContext(), var.mask);	
		BinaryOperator* andInst = BinaryOperator::Create(Instruction::And, final, const_intMask , "Masking", context.currentBlock());
		ConstantInt* const_intShift = ConstantInt::get(getGlobalContext(), var.shft);
		BinaryOperator* shiftInst = BinaryOperator::Create(Instruction::LShr,andInst ,const_intShift, "Shifting", context.currentBlock());
		final = trueSize(shiftInst,context);
	}

	return final;
}

CIdentifier CAliasDeclaration::empty("");

Value* CAliasDeclaration::codeGen(CodeGenContext& context)
{
	return NULL;
}

Value* CMethodCall::codeGen(CodeGenContext& context)
{
	Function *function = context.module->getFunction(id.name.c_str());
	if (function == NULL) 
	{
		std::cerr << "no such function " << id.name << endl;
		context.errorFlagged=true;
		return NULL;
	}
	std::vector<Value*> args;
	ExpressionList::const_iterator it;
	for (it = arguments.begin(); it != arguments.end(); it++) {
		args.push_back((**it).codeGen(context));
	}
	CallInst *call = CallInst::Create(function, args.begin(), args.end(), "", context.currentBlock());
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
	if (baseDivisor.getLimitedValue()==0)
	{
		baseDivisor = APInt(bitWidth,1);
	}

	for (unsigned a=0;a<cnt;a++)
	{
		std::vector<Value*> args;

		APInt tmp = integer.integer.udiv(baseDivisor);

		args.push_back(ConstantInt::get(getGlobalContext(), APInt(32, tbl[tmp.getLimitedValue()])));
		CallInst *call = CallInst::Create(context.debugTraceChar,args.begin(),args.end(),"DEBUGTRACE",context.currentBlock());

		integer.integer-=tmp*baseDivisor;
		if (cnt!=1)
		{
			baseDivisor=baseDivisor.udiv(APInt(bitWidth,currentBase));
		}
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

	Value* loadedValue = ident.codeGen(context);
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
bool CBinaryOperator::IsCarryExpression()
{
	return (op==TOK_ADD || op==TOK_SUB); 
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

		switch (op) 
		{
		case TOK_ADD:
			return BinaryOperator::Create(Instruction::Add,left,right,"",context.currentBlock());
		case TOK_SUB:
			return BinaryOperator::Create(Instruction::Sub,left,right,"",context.currentBlock());
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

Value* CCastOperator::codeGen(CodeGenContext& context)
{
	// Compute a mask between specified bits, shift result down by start bits
	
	// Step 1, get bit size of input operand
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

	std::cerr << "Illegal type in cast" << std::endl;
	
	context.errorFlagged=true;

	return NULL;
}

Value* CRotationOperator::codeGen(CodeGenContext& context)
{
	Value* toShift = value.codeGen(context);
	Value* shiftIn = bitsIn.codeGen(context);
	BitVariable var;
	if (context.locals().find(bitsOut.name) == context.locals().end())
	{
		if (context.globals().find(bitsOut.name) == context.globals().end())
		{
			std::cerr << "undeclared variable " << bitsOut.name << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		else
		{
			var=context.globals()[bitsOut.name];
		}
	}
	else
	{
		var=context.locals()[bitsOut.name];
	}
	
	if (!toShift->getType()->isIntegerTy())
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

	const IntegerType* toShiftType = cast<IntegerType>(toShift->getType());

	if (direction == TOK_ROL)
	{
		APInt rotBy = rotAmount.integer;
		rotBy = rotBy.zextOrTrunc(toShiftType->getBitWidth());
		APInt amountToShift = APInt(toShiftType->getBitWidth(),toShiftType->getBitWidth()) - rotBy;

		Value *shiftedDown = BinaryOperator::Create(Instruction::LShr,toShift,ConstantInt::get(getGlobalContext(),amountToShift),"carryOutShift",context.currentBlock());

		CAssignment::generateAssignment(var,shiftedDown,context);

		Value *shifted=BinaryOperator::Create(Instruction::Shl,toShift,ConstantInt::get(getGlobalContext(),rotBy),"rolShift",context.currentBlock());

		Instruction::CastOps op = CastInst::getCastOpcode(shiftIn,false,toShiftType,false);

		Value *upCast = CastInst::Create(op,shiftIn,toShiftType,"cast",context.currentBlock());

		Value *rotResult = BinaryOperator::Create(Instruction::Or,upCast,shifted,"rolOr",context.currentBlock());

		return rotResult;
	}
	else
	{
		APInt rotBy = rotAmount.integer;
		rotBy = rotBy.zextOrTrunc(toShiftType->getBitWidth());
		APInt amountToShift = APInt(toShiftType->getBitWidth(),toShiftType->getBitWidth()) - rotBy;
		APInt downMask(toShiftType->getBitWidth(),0);
		downMask=~downMask;
		downMask = downMask.lshr(amountToShift);

		Value *maskedDown = BinaryOperator::Create(Instruction::And,toShift,ConstantInt::get(getGlobalContext(),downMask),"carryOutMask",context.currentBlock());

		CAssignment::generateAssignment(var,maskedDown,context);

		Value *shifted=BinaryOperator::Create(Instruction::LShr,toShift,ConstantInt::get(getGlobalContext(),rotBy),"rorShift",context.currentBlock());
			
		Instruction::CastOps op = CastInst::getCastOpcode(shiftIn,false,toShiftType,false);

		Value *upCast = CastInst::Create(op,shiftIn,toShiftType,"cast",context.currentBlock());

		Value *shiftedUp = BinaryOperator::Create(Instruction::Shl,upCast,ConstantInt::get(getGlobalContext(),amountToShift),"rorOrShift",context.currentBlock());
		
		Value *rotResult = BinaryOperator::Create(Instruction::Or,shiftedUp,shifted,"rorOr",context.currentBlock());

		return rotResult;
		std::cerr << "Unsupported rotation" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	return value.codeGen(context);
}

Value* CAssignment::generateAssignment(BitVariable& to, Value* from,CodeGenContext& context)
{
	Value* assignTo;

	assignTo = to.value;

	// Handle variable promotion
	const Type* ty = Type::getIntNTy(getGlobalContext(),to.size.getLimitedValue());
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
		// Now if the assignment is assigning to an aliased register part, we need to have loaded the original register, masked off the inverse of the section mask, and or'd in the result before we store
		LoadInst* loadInst=new LoadInst(to.value, "", false, context.currentBlock());
		ConstantInt* const_intInvMask = ConstantInt::get(getGlobalContext(), ~to.mask);
		BinaryOperator* primaryAndInst = BinaryOperator::Create(Instruction::And, loadInst, const_intInvMask , "InvMasking", context.currentBlock());
		final = BinaryOperator::Create(Instruction::Or,primaryAndInst,andInst,"Combining",context.currentBlock());
	}

	return new StoreInst(final, assignTo, false, context.currentBlock());
}


Value* CAssignment::codeGen(CodeGenContext& context)
{
	BitVariable var;
	Value* assignWith;
	if (context.locals().find(lhs.name) == context.locals().end())
	{
		if (context.globals().find(lhs.name) == context.globals().end())
		{
			std::cerr << "undeclared variable " << lhs.name << endl;
			context.errorFlagged=true;
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

	if (var.mappingRef)
	{
		std::cerr << "Cannot perform operation on a mappingRef" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	assignWith = rhs.codeGen(context);

	return CAssignment::generateAssignment(var,assignWith,context);
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

Value* CStatesDeclaration::codeGen(CodeGenContext& context)
{
	Twine numStatesTwine(states.size());
	std::string numStates = numStatesTwine.str();
	APInt overSized(4*numStates.length(),numStates,10);
	unsigned bitsNeeded = overSized.getActiveBits();

	if (context.currentState() == NULL)
	{
		if (context.parentHandler==NULL)
		{
			std::cerr << "It is illegal to declare STATES outside of a handler!" << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		context.parentHandler->child = this;
	}
	else
	{
		CStatesDeclaration* parentStates = context.currentState();
		std::string stateForChild=context.stateLabelStack.substr(context.stateLabelStack.rfind(".")+1);
		CStateDeclaration* parentState = parentStates->getStateDeclaration(stateForChild);
		if (parentState == NULL)
		{
			std::cerr << "Unable to find parent state - this is impossible, unless i've done something badly wrong!" << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		parentState->child=this;
	}

	std::string stkStatePLbl = "STACKP" + context.stateLabelStack;
	std::string stkStateNLbl = "STACKN" + context.stateLabelStack;
	std::string idxStateLbl = "IDX" + context.stateLabelStack;
	std::string curStateLbl = "CUR" + context.stateLabelStack;
	std::string nxtStateLbl = "NEXT" + context.stateLabelStack;
	std::string stateLabel = "STATE" + context.stateLabelStack;

	GlobalVariable *curState = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),bitsNeeded), false, GlobalValue::InternalLinkage,NULL,curStateLbl);
	GlobalVariable *nxtState = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),bitsNeeded), false, GlobalValue::InternalLinkage,NULL,nxtStateLbl);
	
	ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(getGlobalContext(), bitsNeeded), MAX_SUPPORTED_STACK_DEPTH);
       	GlobalVariable* stkStateP = new GlobalVariable(*context.module, ArrayTy_0, false,  GlobalValue::InternalLinkage, NULL, stkStatePLbl);
       	GlobalVariable* stkStateN = new GlobalVariable(*context.module, ArrayTy_0, false,  GlobalValue::InternalLinkage, NULL, stkStateNLbl);
	GlobalVariable *stkStateIdx = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),MAX_SUPPORTED_STACK_BITS), false, GlobalValue::InternalLinkage,NULL,idxStateLbl);

 
	StateVariable newStateVar;
	newStateVar.currentState = curState;
	newStateVar.nextState = nxtState;
	newStateVar.stateStackPrev = stkStateP;
	newStateVar.stateStackNext = stkStateN;
	newStateVar.stateStackIndex = stkStateIdx;
	newStateVar.decl = this;

	context.states()[stateLabel]=newStateVar;
	context.statesAlt()[this]=newStateVar;

	// Constant Definitions
	ConstantInt* const_int32_n = ConstantInt::get(getGlobalContext(), APInt(bitsNeeded, 0, false));
	ConstantInt* const_int4_n = ConstantInt::get(getGlobalContext(), APInt(MAX_SUPPORTED_STACK_BITS, 0, false));
	ConstantAggregateZero* const_array_n = ConstantAggregateZero::get(ArrayTy_0);

	// Global Variable Definitions
	curState->setInitializer(const_int32_n);
	nxtState->setInitializer(const_int32_n);
	stkStateIdx->setInitializer(const_int4_n);
	stkStateP->setInitializer(const_array_n);
	stkStateN->setInitializer(const_array_n);

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
		BranchInst::Create(states[a]->exit,states[a]->entry);
		BranchInst::Create(exitState,states[a]->exit);			// this terminates the final blocks from our states
	}

	return NULL;
}

Value* CVariableDeclaration::codeGen(CodeGenContext& context)
{
	BitVariable temp;

	temp.size = size.integer;
	temp.trueSize = size.integer;
	temp.cnst = APInt(size.integer.getLimitedValue(),0);
	temp.mask = ~temp.cnst;
	temp.shft = APInt(size.integer.getLimitedValue(),0);
	temp.aliased = false;
	temp.mappingRef = false;

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
			alias.mappingRef=false;

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
	
			context.pushBlock(pState->entry);
			block.codeGen(context);
			pState->entry=context.currentBlock();
			context.popBlock();

			context.stateLabelStack=oldStateLabelStack;
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
	
	std::string depth = id.name + "DEPTH";
	std::string depthIdx = id.name + "DEPTHIDX";

	ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(getGlobalContext(), MAX_SUPPORTED_STACK_BITS), MAX_SUPPORTED_STACK_DEPTH);
       	depthTree = new GlobalVariable(*context.module, ArrayTy_0, false,  GlobalValue::InternalLinkage, NULL, depth);
	depthTreeIdx = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),MAX_SUPPORTED_STACK_BITS), false, GlobalValue::InternalLinkage,NULL,depthIdx);
	
	// Constant Definitions
	ConstantInt* const_int4_n = ConstantInt::get(getGlobalContext(), APInt(MAX_SUPPORTED_STACK_BITS, 0, false));
	ConstantAggregateZero* const_array_n = ConstantAggregateZero::get(ArrayTy_0);

	// Global Variable Definitions
	depthTreeIdx->setInitializer(const_int4_n);
	depthTree->setInitializer(const_array_n);

	context.m_handlers[id.name]=this;

	context.pushBlock(bblock);

	std::string oldStateLabelStack = context.stateLabelStack;
	context.stateLabelStack+="." + id.name;

	context.parentHandler=this;

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
	
Value* CInstruction::codeGen(CodeGenContext& context)
{
	vector<const Type*> argTypes;

	// First up, get the first operand (this must be a computable constant!) - remaining operands are only used for asm/disasm generation

	unsigned numOpcodes = operands[0]->GetNumComputableConstants(context);
	if (numOpcodes==0)
	{
		std::cerr << "Opcode for instruction must be able to generate constants" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	for (unsigned a=0;a<numOpcodes;a++)
	{
		APInt opcode = operands[0]->GetComputableConstant(context,a);

// Generate quick disasm function for this opcode (this is currently crude and only does the Opcode)

		PointerType* PointerTy_2 = PointerType::get(IntegerType::get(getGlobalContext(), 8), 0);
  
  		FunctionType* ftypeDeb = FunctionType::get(/*Result=*/PointerTy_2,/*Params=*/argTypes,/*isVarArg=*/false);
		Function* functionDeb = Function::Create(ftypeDeb, GlobalValue::ExternalLinkage, "DIS_"+opcode.toString(16,false),context.module);
		BasicBlock *bblockDeb = BasicBlock::Create(getGlobalContext(), "entry", functionDeb, 0);

		context.pushBlock(bblockDeb);

		///do quick trace
		CDebugTraceString opcodeString(processMnemonic(context,mnemonic,opcode,a));

		ReturnInst::Create(getGlobalContext(),opcodeString.string.codeGen(context),context.currentBlock());

		context.popBlock();

// End disasm function

		FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext()),argTypes, false);
		Function* function = Function::Create(ftype, GlobalValue::InternalLinkage, "OPCODE_"+opcodeString.string.quoted.substr(1,mnemonic.quoted.length()-2) + "_" + opcode.toString(16,false),context.module);
		BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", function, 0);

		context.pushBlock(bblock);

		operands[0]->DeclareLocal(context,a);

		block.codeGen(context);

		ReturnInst::Create(getGlobalContext(), context.currentBlock());			/* block may well have changed by time we reach here */

		context.popBlock();

		// Glue callee back into execute (assumes execute comes before instructions at all times for now)
		BasicBlock* tempBlock = BasicBlock::Create(getGlobalContext(),"callOut" + opcode.toString(16,false),context.blockEndForExecute->getParent(),0);
		std::vector<Value*> args;
		CallInst* fcall = CallInst::Create(function,args.begin(),args.end(),"",tempBlock);
		BranchInst::Create(context.blockEndForExecute,tempBlock);
		context.switchForExecute->addCase(ConstantInt::get(getGlobalContext(),opcode),tempBlock);

	}
	return NULL;
}

Value* CExecute::codeGen(CodeGenContext& context)
{
	Value* load = opcode.codeGen(context);
	load = opcode.trueSize(load,context);
	
	if (load->getType()->isIntegerTy())
	{
		const IntegerType* typeForExecute = cast<IntegerType>(load->getType());

		context.blockEndForExecute = BasicBlock::Create(getGlobalContext(), "execReturn", context.currentBlock()->getParent(), 0);		// Need to cache this block away somewhere
	
		context.switchForExecute = SwitchInst::Create(load,context.blockEndForExecute,2<<typeForExecute->getBitWidth(),context.currentBlock());

		context.setBlock(context.blockEndForExecute);
	}
	return NULL;
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
			std::cerr << "Unable to find HANDLER/SUBSTATE " << findString << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
	
		StateVariable state = context.states()[findString];

		int stateIndex = state.decl->getStateDeclarationIndex(stateIdents[b+1]->name);
		if (stateIndex == -1)
		{
			std::cerr << "Unable to find value for state " << stateIdents[b+1]->name << std::endl;
			context.errorFlagged=true;
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

// Language decision : NEXT TEST.A.B will set TEST.A and TEST.A.B states

Value* CStateJump::codeGen(CodeGenContext& context)
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
			std::cerr << "Unable to find HANDLER/SUBSTATE " << findString << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
	
		StateVariable state = context.states()[findString];

		int stateIndex = state.decl->getStateDeclarationIndex(stateIdents[b+1]->name);
		if (stateIndex == -1)
		{
			std::cerr << "Unable to find value for state " << stateIdents[b+1]->name << std::endl;
			context.errorFlagged=true;
			return NULL;
		}

		Twine numStatesTwine(state.decl->states.size());
		std::string numStates = numStatesTwine.str();
		APInt overSized(4*numStates.length(),numStates,10);
		unsigned bitsNeeded = overSized.getActiveBits();
	
		// We need to store new state into the next state register
		res = new StoreInst(ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,stateIndex)),state.nextState,false,context.currentBlock());
	}

	return res;
}

Value* CStatePush::codeGen(CodeGenContext& context)
{
	unsigned int a,b;
	unsigned int numStatesToCheck = stateIdents.size()-1;
	Value *res=NULL;

	// Step 1, get handler ptr so we can get access to the depth stack
	if (context.m_handlers.find(stateIdents[0]->name) == context.m_handlers.end())
	{
		std::cerr << "Unable to find HANDLER " << stateIdents[0]->name << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	CHandlerDeclaration* handler = context.m_handlers[stateIdents[0]->name];

	// Step 2, compute depth and push to depth stack
	
	Value* dIndex= new LoadInst(handler->depthTreeIdx,"depthIndex",false,context.currentBlock());
	std::vector<Value*> indices;
	ConstantInt* const_intn = ConstantInt::get(getGlobalContext(), APInt(MAX_SUPPORTED_STACK_BITS, StringRef("0"), 10));
	indices.push_back(const_intn);
	indices.push_back(dIndex);
	Value* dRef = GetElementPtrInst::Create(handler->depthTree, indices.begin(), indices.end(),"depth",context.currentBlock());

	new StoreInst(ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,numStatesToCheck)),dRef,false,context.currentBlock());

	Value* dInc = BinaryOperator::Create(Instruction::Add,dIndex,ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,1)),"incrementIndex",context.currentBlock());
	new StoreInst(dInc,handler->depthTreeIdx,false,context.currentBlock());

	// Now iterate through state stacks, and push the relevant states
	for (b=0;b<numStatesToCheck;b++)
	{
		std::string findString = "STATE";
		for (a=0;a<b+1;a++)
		{
			findString+="." + stateIdents[a]->name;
		}

		if (context.states().find(findString) == context.states().end())
		{
			std::cerr << "Unable to find HANDLER/SUBSTATE " << findString << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
	
		StateVariable state = context.states()[findString];

		int stateIndex = state.decl->getStateDeclarationIndex(stateIdents[b+1]->name);
		if (stateIndex == -1)
		{
			std::cerr << "Unable to find value for state " << stateIdents[b+1]->name << std::endl;
			context.errorFlagged=true;
			return NULL;
		}

		Twine numStatesTwine(state.decl->states.size());
		std::string numStates = numStatesTwine.str();
		APInt overSized(4*numStates.length(),numStates,10);
		unsigned bitsNeeded = overSized.getActiveBits();
	
		// We need to save away the current next pointer onto the stack and the new next onto the next stack, then modify next

		Value* index= new LoadInst(state.stateStackIndex,"stackIndex",false,context.currentBlock());
		std::vector<Value*> indices;
		ConstantInt* const_intn = ConstantInt::get(getGlobalContext(), APInt(bitsNeeded, StringRef("0"), 10));
		indices.push_back(const_intn);
		indices.push_back(index);
		Value* refP = GetElementPtrInst::Create(state.stateStackPrev, indices.begin(), indices.end(),"stackPosP",context.currentBlock());
		Value* refN = GetElementPtrInst::Create(state.stateStackNext, indices.begin(), indices.end(),"stackPosN",context.currentBlock());

		Value* curNext = new LoadInst(state.nextState,"currentNext",false,context.currentBlock());

		new StoreInst(curNext,refP,false,context.currentBlock());
		new StoreInst(ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,stateIndex)),refN,false,context.currentBlock());

		Value* inc = BinaryOperator::Create(Instruction::Add,index,ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,1)),"incrementIndex",context.currentBlock());
		new StoreInst(inc,state.stateStackIndex,false,context.currentBlock());

		res = new StoreInst(ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,stateIndex)),state.nextState,false,context.currentBlock());
	}

	return res;
}
	
void CStatePop::StateWalker(CodeGenContext& context,CStatesDeclaration* iterate,Value* depth)
{
	if (iterate)
	{
		StateVariable stateVar = context.statesAlt()[iterate];
		Twine numStatesTwine(stateVar.decl->states.size());
		std::string numStates = numStatesTwine.str();
		APInt overSized(4*numStates.length(),numStates,10);
		unsigned bitsNeeded = overSized.getActiveBits();

		// We need to pop from our stack and put next back
		Value* index= new LoadInst(stateVar.stateStackIndex,"stackIndex",false,context.currentBlock());
		Value* dec = BinaryOperator::Create(Instruction::Sub,index,ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,1)),"decrementIndex",context.currentBlock());
		new StoreInst(dec,stateVar.stateStackIndex,false,context.currentBlock());

		std::vector<Value*> indices;
		ConstantInt* const_intn = ConstantInt::get(getGlobalContext(), APInt(bitsNeeded, StringRef("0"), 10));
		indices.push_back(const_intn);
		indices.push_back(dec);
		Value* refP = GetElementPtrInst::Create(stateVar.stateStackPrev, indices.begin(), indices.end(),"stackPosP",context.currentBlock());
		Value* refN = GetElementPtrInst::Create(stateVar.stateStackNext, indices.begin(), indices.end(),"stackPosN",context.currentBlock());

		Value* prevState = new LoadInst(refP,"oldNext",false,context.currentBlock());
		Value* oldNext = new LoadInst(refN,"oldNext",false,context.currentBlock());

		new StoreInst(prevState,stateVar.nextState,false,context.currentBlock());

		// Now decrement our depth marker, if its zero, exit - else further down the rabbit hole
		Value* decDepth = BinaryOperator::Create(Instruction::Sub,depth,ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,1)),"decrementDepth",context.currentBlock());

		BasicBlock* switchBlock = BasicBlock::Create(getGlobalContext(), "descend",context.currentBlock()->getParent(),0);
		BasicBlock* exitState = BasicBlock::Create(getGlobalContext(), "popout", context.currentBlock()->getParent(), 0);

		Value* checkDepth = CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,decDepth, ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,0)), "checkDepth", context.currentBlock());
		BranchInst::Create(exitState,switchBlock,checkDepth,context.currentBlock());
		
		SwitchInst* void_6 = SwitchInst::Create(oldNext,exitState,iterate->states.size(), switchBlock);

		// Need to add cases, then for each case descend (assuming there is anyway to descend too!)
		for (int a=0;a<iterate->states.size();a++)
		{
			if (iterate->states[a]->child)
			{
				BasicBlock* bb = BasicBlock::Create(getGlobalContext(),"",context.currentBlock()->getParent(),0);
				ConstantInt* tt = ConstantInt::get(getGlobalContext(),APInt(bitsNeeded,a,false));
				void_6->addCase(tt,bb);
				context.pushBlock(bb);
				StateWalker(context,iterate->states[a]->child,decDepth);
				BranchInst::Create(exitState,context.currentBlock());
				context.popBlock();
			}
		}

		context.setBlock(exitState);
	}
}

Value* CStatePop::codeGen(CodeGenContext& context)
{
	unsigned int a,b;
	Value *res=NULL;

	// Step 1, get handler ptr so we can get access to the depth stack
	if (context.m_handlers.find(ident.name) == context.m_handlers.end())
	{
		std::cerr << "Unable to find HANDLER " << ident.name << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	CHandlerDeclaration* handler = context.m_handlers[ident.name];

	// Step 2, compute depth and push to depth stack
	
	Value* dIndex= new LoadInst(handler->depthTreeIdx,"depthIndex",false,context.currentBlock());
	Value* dDec = BinaryOperator::Create(Instruction::Sub,dIndex,ConstantInt::get(getGlobalContext(),APInt(MAX_SUPPORTED_STACK_BITS,1)),"decrementIndex",context.currentBlock());
	new StoreInst(dDec,handler->depthTreeIdx,false,context.currentBlock());
	std::vector<Value*> indices;
	ConstantInt* const_intn = ConstantInt::get(getGlobalContext(), APInt(MAX_SUPPORTED_STACK_BITS, StringRef("0"), 10));
	indices.push_back(const_intn);
	indices.push_back(dDec);
	Value* dRef = GetElementPtrInst::Create(handler->depthTree, indices.begin(), indices.end(),"depth",context.currentBlock());

	Value* statesToPop=new LoadInst(dRef,"numStates",false,context.currentBlock());

	// We now know how far down the tree we must go to restore the states position.
	// the number of states on a handler is fixed, however the depth is not, which means we need to have a way to generate code
	// that correctly handles the various depths it may need to walk.

	// Yuk, so first off pop the main handler state, as this will tell us which state stack to pop off next, we keep going untill we run out
	
	CStatesDeclaration *iterate = handler->child;
	StateWalker(context,iterate,statesToPop);

	return NULL;
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

		CIdentifier* identifier = cast<CIdentifier>(&mapping->expr);

		if (context.locals().find(identifier->name) == context.locals().end())
		{
			if (context.globals().find(identifier->name) == context.globals().end())
			{
				std::cerr << "undeclared variable " << identifier->name << endl;
				context.errorFlagged=true;
				return temp;
			}
			else
			{
				temp=context.globals()[identifier->name];
			}
		}
		else
		{
			temp=context.locals()[identifier->name];
		}
	}
	else
	{
		// Not an identifier, so we must create an expression maps (this limits the mapping to only being useful for a limited set of things, (but is an accepted part of the language!)

		temp.mappingRef=true;
		temp.value = mapping->expr.codeGen(context);
	}
}

llvm::APInt COperandMapping::GetComputableConstant(CodeGenContext& context,unsigned num)
{
	if (context.m_mappings.find(ident.name)==context.m_mappings.end())
	{
		std::cerr << "Undeclared mapping : " << ident.name << std::endl;
		context.errorFlagged=true;
		return APInt(0,0,false);
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

const CString* COperandPartial::GetString(CodeGenContext& context,unsigned num,unsigned slot)
{
	for (int a=operands.size()-1;a>=0;a--)
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
	APInt result(0,0,false);

	for (int a=operands.size()-1;a>=0;a--)
	{
		unsigned tNum=num % operands[a]->GetNumComputableConstants(context);
		APInt temp = operands[a]->GetComputableConstant(context,tNum);
		num/=operands[a]->GetNumComputableConstants(context);
		unsigned curBitWidth = result.getBitWidth();

		result=result.zext(result.getBitWidth()+temp.getBitWidth());
		temp=temp.zext(result.getBitWidth());
		temp=temp.shl(curBitWidth);
		result=result.Or(temp);
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

CInteger CAffect::emptyParam(stringZero);

Value* CAffect::codeGen(CodeGenContext& context,Value* exprResult,Value* lhs,Value* rhs,int optype)
{
	BitVariable var;
	if (context.locals().find(ident.name) == context.locals().end())
	{
		if (context.globals().find(ident.name) == context.globals().end())
		{
			std::cerr << "undeclared variable " << ident.name << std::endl;
			context.errorFlagged=true;
			return NULL;
		}
		else
		{
			var=context.globals()[ident.name];
		}
	}
	else
	{
		var=context.locals()[ident.name];
	}
	
	if (var.mappingRef)
	{
		std::cerr << "Cannot perform operation on a mappingRef" << std::endl;
		context.errorFlagged=true;
		return NULL;
	}

	const IntegerType* resultType = cast<IntegerType>(exprResult->getType());
	Value *answer;
	switch (type)
	{
		case TOK_ZERO:
			answer=CmpInst::Create(Instruction::ICmp,ICmpInst::ICMP_EQ,exprResult, ConstantInt::get(getGlobalContext(),APInt(resultType->getBitWidth(),0,false)), "", context.currentBlock());
			break;
		case TOK_SIGN:
			{
				APInt signBit(resultType->getBitWidth(),0,false);
				signBit.setBit(resultType->getBitWidth()-1);
				answer=BinaryOperator::Create(Instruction::And,exprResult,ConstantInt::get(getGlobalContext(),signBit),"",context.currentBlock());
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
						const Type* ty = Type::getIntNTy(getGlobalContext(),computeClosestPower2Size.getLimitedValue());
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
				const Type* ty = Type::getIntNTy(getGlobalContext(),16);
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
			{
				APInt bit(resultType->getBitWidth(),0,false);
				APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
				bit.setBit(param.integer.getLimitedValue());
				answer=BinaryOperator::Create(Instruction::And,exprResult,ConstantInt::get(getGlobalContext(),bit),"",context.currentBlock());
				answer=BinaryOperator::Create(Instruction::LShr,answer,ConstantInt::get(getGlobalContext(),shift),"",context.currentBlock());
			}
			break;

		case TOK_CARRY:
			{
				if (lhs==NULL || rhs==NULL)
				{
					std::cerr << "CARRY is not supported for non carry expressions" << std::endl;
					context.errorFlagged=true;
					return NULL;
				}
				if (param.integer.getLimitedValue() >= resultType->getBitWidth())
				{
					std::cerr << "Bit to carry is outside of range for result" << std::endl;
					context.errorFlagged=true;
					return NULL;
				}
				
				const IntegerType* lhsType = cast<IntegerType>(lhs->getType());
				const IntegerType* rhsType = cast<IntegerType>(rhs->getType());
		       		
				Instruction::CastOps lhsOp = CastInst::getCastOpcode(lhs,false,resultType,false);
				lhs = CastInst::Create(lhsOp,lhs,resultType,"cast",context.currentBlock());
				Instruction::CastOps rhsOp = CastInst::getCastOpcode(rhs,false,resultType,false);
				rhs = CastInst::Create(rhsOp,rhs,resultType,"cast",context.currentBlock());

				if (optype==TOK_ADD)
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

				APInt bit(resultType->getBitWidth(),0,false);
				APInt shift = param.integer.zextOrTrunc(resultType->getBitWidth());
				bit.setBit(param.integer.getLimitedValue());
				ConstantInt* bitC=ConstantInt::get(getGlobalContext(),bit);
				ConstantInt* shiftC=ConstantInt::get(getGlobalContext(),shift);

				answer=BinaryOperator::Create(Instruction::And,answer,bitC,"",context.currentBlock());
				answer=BinaryOperator::Create(Instruction::LShr,answer,shiftC,"",context.currentBlock());
			}
			break;

		default:
			std::cerr << "Unknown affector" << std::endl;
			context.errorFlagged=true;
			return NULL;
	}

	CAssignment::generateAssignment(var,answer,context);

	return NULL;
}

Value* CAffector::codeGen(CodeGenContext& context)
{
	Value* left=NULL;
	Value* right=NULL;
	Value* exprResult=NULL;
	int type=0;

	if (expr.IsCarryExpression())
	{
		CBinaryOperator* oper = cast<CBinaryOperator>(&expr);
		left=oper->lhs.codeGen(context);
		right=oper->rhs.codeGen(context);

		type=oper->op;
		exprResult = oper->codeGen(context,left,right);
	}
	else
	{
		exprResult = expr.codeGen(context);
	}

	for (int a=0;a<affectors.size();a++)
	{
		affectors[a]->codeGen(context,exprResult,left,right,type);
	}

	return exprResult;
}

