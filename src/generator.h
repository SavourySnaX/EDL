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
class CHandlerDeclaration;
class CMappingDeclaration;

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
	bool	mappingRef;
	int	pinType;
	Instruction	*writeAccessor;
};

class StateVariable
{
public:
	Value* currentState;
	Value* nextState;
	Value* stateStackPrev;
	Value* stateStackNext;
	Value* stateStackIndex;
	CStatesDeclaration* decl;
};

class ExecuteInformation
{
public:
    llvm::BasicBlock* blockEndForExecute;
    llvm::SwitchInst* switchForExecute;	
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

    CHandlerDeclaration* parentHandler;

    Module *module;
    ExecutionEngine		*ee;
    CodeGenContext() { module = new Module("main", getGlobalContext()); }
    Function *debugTraceString;
    Function *debugTraceChar;

    bool errorFlagged;

    std::vector<ExecuteInformation> executeLocations;

    std::map<std::string, BitVariable> m_globals;
    std::map<std::string, StateVariable> m_states;
    std::map<std::string, Function*> m_externFunctions;
    std::map<CStatesDeclaration*,StateVariable> m_statesAlt;
    std::map<std::string, CHandlerDeclaration*> m_handlers;
    std::map<std::string, CMappingDeclaration*> m_mappings;

    void generateCode(CBlock& root);
    GenericValue runCode();
    std::map<std::string, BitVariable>& locals() { return blocks.top()->locals; }
    std::map<std::string, BitVariable>& globals() { return m_globals; }
    std::map<std::string, StateVariable>& states() { return m_states; }
    std::map<CStatesDeclaration*, StateVariable>& statesAlt() { return m_statesAlt; }
    BasicBlock *currentBlock() { if (blocks.size()>0) return blocks.top()->block; else return NULL; }
    void setBlock(BasicBlock *block) { blocks.top()->block = block; }
    void pushBlock(BasicBlock *block) 
    { 
	    std::map<std::string,BitVariable> tLocals; 
	    if (blocks.size()>0) 
		    tLocals = blocks.top()->locals; 
	    blocks.push(new CodeGenBlock()); 
	    blocks.top()->block = block; 
	    blocks.top()->locals = tLocals;
    }
    void popBlock() { CodeGenBlock *top = blocks.top(); blocks.pop(); delete top; }
    CStatesDeclaration* currentState() { if (statesStack.empty()) return NULL; else return statesStack.top(); }
    void pushState(CStatesDeclaration *state) { statesStack.push(state); }
    void popState() { statesStack.pop(); }
};
