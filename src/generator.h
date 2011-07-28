#include <stack>
#include <typeinfo>
#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/Type.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/PassManager.h>
#include <llvm/Instructions.h>
#include <llvm/CallingConv.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Support/IRBuilder.h>
//#include <llvm/ModuleProvider.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

class CBlock;
class CStatesDeclaration;

class BitVariable
{
public:
	Value* value;
	APInt	size;
	APInt	trueSize;
	APInt	mask;
	APInt	cnst;
	APInt	shft;
	bool	aliased;
};

class StateVariable
{
public:
	Value* value;
	CStatesDeclaration* decl;
};

class CodeGenBlock 
{
public:
    BasicBlock *block;
    std::map<std::string, BitVariable> locals;
};

class CodeGenContext 
{
    std::stack<CodeGenBlock *> blocks;
    std::stack<CStatesDeclaration *> statesStack;
    Function *mainFunction;

public:
    std::string stateLabelStack;
    Module *module;
    ExecutionEngine		*ee;
    CodeGenContext() { module = new Module("main", getGlobalContext()); }
    Function *debugTraceString;
    Function *debugTraceChar;
    Function *handlerToTest;		/* ONLY USED FOR TESTING PURPOSES */

    std::map<std::string, BitVariable> m_globals;
    std::map<std::string, StateVariable> m_states;

    void generateCode(CBlock& root);
    GenericValue runCode();
    std::map<std::string, BitVariable>& locals() { return blocks.top()->locals; }
    std::map<std::string, BitVariable>& globals() { return m_globals; }
    std::map<std::string, StateVariable>& states() { return m_states; }
    BasicBlock *currentBlock() { if (blocks.size()>0) return blocks.top()->block; else return NULL; }
    void setBlock(BasicBlock *block) { blocks.top()->block = block; }
    void pushBlock(BasicBlock *block) 
    { 
	    std::cout << "Pushing new Block " << std::endl;
	    std::map<std::string,BitVariable> tLocals; 
	    if (blocks.size()>0) 
		    tLocals = blocks.top()->locals; 
	    blocks.push(new CodeGenBlock()); 
	    blocks.top()->block = block; 
	    blocks.top()->locals = tLocals;
	    std::cout << "Num Locals : " << tLocals.size() << std::endl;
    }
    void popBlock() { std::cout << "Popping old Block" << std::endl; CodeGenBlock *top = blocks.top(); blocks.pop(); delete top; }
    CStatesDeclaration* currentState() { if (statesStack.empty()) return NULL; else return statesStack.top(); }
    void pushState(CStatesDeclaration *state) { statesStack.push(state); }
    void popState() { statesStack.pop(); }
};
