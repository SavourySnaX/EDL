#include "ast.h"
#include "generator.h"
#include "parser.hpp"

using namespace std;

/* Compile the AST into a module */
void CodeGenContext::generateCode(CBlock& root)
{
	std::cout << "Generating code...\n";
	
	ee = EngineBuilder(module).create();

	root.codeGen(*this);	/* Generate complete code - starting at no block (global space) */


	/* Finally create an entry point - which does nothing for now */
	vector<const Type*> argTypes;
	FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext())/* Type::getInt64Ty(getGlobalContext())*/, argTypes, false);
	mainFunction = Function::Create(ftype, GlobalValue::ExternalLinkage, "main", module);
	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", mainFunction, 0);
	
	/* Push a new variable/block context */
	pushBlock(bblock);
//	root.codeGen(*this); /* emit bytecode for the toplevel block */
	ReturnInst::Create(getGlobalContext(), /*root.codeGen(*this), */bblock);
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

Value* CIdentifier::codeGen(CodeGenContext& context)
{
	std::cout << "Creating identifier reference: " << name << endl;
	if (context.locals().find(name) == context.locals().end()) {
		std::cerr << "undeclared variable " << name << endl;
		return NULL;
	}
	return new LoadInst(context.locals()[name], "", false, context.currentBlock());
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

Value* CBinaryOperator::codeGen(CodeGenContext& context)
{
	std::cout << "Creating binary operation " << op << endl;
#if 0
	Instruction::BinaryOps instr;
	switch (op) {
		case TPLUS: 	instr = Instruction::Add; goto math;
		case TMINUS: 	instr = Instruction::Sub; goto math;
		case TMUL: 		instr = Instruction::Mul; goto math;
		case TDIV: 		instr = Instruction::SDiv; goto math;
				
		/* TODO comparison */
	}
#endif
	return NULL;
#if 0
math:
	return BinaryOperator::Create(instr, lhs.codeGen(context), 
		rhs.codeGen(context), "", context.currentBlock());
#endif
}

Value* CAssignment::codeGen(CodeGenContext& context)
{
	std::cout << "Creating assignment for " << lhs.name << endl;
	if (context.locals().find(lhs.name) == context.locals().end()) {
		std::cerr << "undeclared variable " << lhs.name << endl;
		return NULL;
	}
	return new StoreInst(rhs.codeGen(context), context.locals()[lhs.name], false, context.currentBlock());
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

Value* CStateDeclaration::codeGen(CodeGenContext& context)
{
	std::cout << "Creating state enumeration " << id.name << "ignored" << endl;

	return NULL;
}

Value* CStatesDeclaration::codeGen(CodeGenContext& context)
{
	Twine numStatesTwine(states.size());
	std::string numStates = numStatesTwine.str();
	unsigned bitsNeeded = APInt::getBitsNeeded(numStates,10)+1;		// +1 because of sign bit (in debug output)
	
	std::cout << "Generating state machine entry " << endl;

	std::cout << "Number of states : " << numStates << " " << bitsNeeded << endl;
	
	GlobalVariable *global = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),bitsNeeded), false, GlobalValue::InternalLinkage,NULL,"STATE" + context.currentBlock()->getParent()->getName());
  
	// Constant Definitions
	ConstantInt* const_int32_n = ConstantInt::get(getGlobalContext(), APInt(bitsNeeded, numStates, 10));
  
	// Global Variable Definitions
	global->setInitializer(const_int32_n);

	return global;
}

Value* CVariableDeclaration::codeGen(CodeGenContext& context)
{
	std::cout << "Creating variable declaration " << id.name << " " << size.integer.toString(10,false) << endl;

	if (context.currentBlock())
	{
		// Within a basic block - so must be a stack variable
		AllocaInst *alloc = new AllocaInst(Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()), id.name.c_str(), context.currentBlock());
		context.locals()[id.name] = alloc;
		return alloc;
	}
	else
	{
		GlobalVariable *global = new GlobalVariable(*context.module,Type::getIntNTy(getGlobalContext(),size.integer.getLimitedValue()), false, GlobalValue::CommonLinkage,NULL,id.name.c_str());
  
		// Constant Definitions
		ConstantInt* const_intn_0 = ConstantInt::get(getGlobalContext(), APInt(size.integer.getLimitedValue(), StringRef("0"), 10));
  
		// Global Variable Definitions
		global->setInitializer(const_intn_0);

//		context.globals()[id.name]=global;
		return global;
	}
#if 0
	if (assignmentExpr != NULL) {
		NAssignment assn(id, *assignmentExpr);
		assn.codeGen(context);
	}
#endif
	return NULL;
}

extern CodeGenContext* globalContext;

Value* CHandlerDeclaration::codeGen(CodeGenContext& context)
{
	vector<const Type*> argTypes;
	FunctionType *ftype = FunctionType::get(Type::getVoidTy(getGlobalContext()),argTypes, false);
	Function* function = Function::Create(ftype, GlobalValue::ExternalLinkage, id.name.c_str(), context.module);
	BasicBlock *bblock = BasicBlock::Create(getGlobalContext(), "entry", function, 0);

	context.pushBlock(bblock);

	block.codeGen(context);

	ReturnInst::Create(getGlobalContext(), bblock);

	context.popBlock();

	std::cout << "Creating function: " << id.name << endl;

	return function;
}

