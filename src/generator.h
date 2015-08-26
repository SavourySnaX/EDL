#include <stack>
#include <typeinfo>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/PassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/IRBuilder.h>
//#include <llvm/ModuleProvider.h>
#include <llvm/Support/TargetSelect.h>
//#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

class CBlock;
class CStatesDeclaration;
class CHandlerDeclaration;
class CMappingDeclaration;
class CIdentifier;
class CodeGenContext;
class CExpression;
class CAffect;

typedef std::vector<CAffect*> AffectorList;

class BitVariable
{
public:
	Value* value;
	APInt	arraySize;
	APInt	size;
	APInt	trueSize;
	APInt	mask;
	APInt	cnst;
	APInt	shft;
	bool	aliased;
	bool	mappingRef;
	CExpression* mapping;
	bool	fromExternal;
	int	pinType;
	Instruction**	writeAccessor;
	Value*		writeInput;
	Value*		priorValue;
	Value*		impedance;
};

class StateVariable
{
public:
	Value* currentState;
	Value* nextState;
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

class CompilerOptions
{
public:

	CompilerOptions() { symbolModifier=NULL; inputFile=NULL; optimisationLevel=0; traceUnimplemented=0; generateDisassembly=1; }
	
	char *symbolModifier;
	char *inputFile;
	int optimisationLevel;
	int traceUnimplemented;
	int generateDisassembly;
};

class CodeGenContext 
{
    std::stack<CodeGenBlock *> blocks;
    std::stack<CStatesDeclaration *> statesStack;
    std::stack<const CIdentifier *> identifierStack;
    Function *mainFunction;

    void GenerateDisassmTables();

public:
    std::string stateLabelStack;

    CHandlerDeclaration* parentHandler;

    Module *module;
    bool isRoot;
    ExecutionEngine		*ee;
    CodeGenContext(CodeGenContext* parent);
    Function *debugTraceString;
    Function *debugTraceChar;
    Function *debugTraceMissing;

    std::string	symbolPrepend;
    std::string moduleName;

    bool errorFlagged;

    std::map<std::string, std::vector<ExecuteInformation> > executeLocations;

    AffectorList	curAffectors;

    struct myAPIntCompare
    {
	bool operator()(const APInt& s1,const APInt& s2) const
	{
		APInt f1=s1;
		APInt f2=s2;

		if (f1.getBitWidth()!=f2.getBitWidth())
		{
			if (f1.getBitWidth()<f2.getBitWidth())
			{
				f1=f1.zext(f2.getBitWidth());
			}
			else
			{
				f2=f2.zext(f1.getBitWidth());
			}
		}
		return f1.ult(f2);
	}
    };

    std::map<std::string, std::map<APInt,std::string,myAPIntCompare> >     disassemblyTable;

    Function* LookupFunctionInExternalModule(const std::string& module, const std::string& name);
    bool LookupBitVariable(BitVariable& outVar,const std::string& module, const std::string& name);

    std::map<std::string, CodeGenContext*> m_includes;
    std::map<std::string, BitVariable> m_globals;
    std::map<std::string, StateVariable> m_states;
    std::map<std::string, Function*> m_externFunctions;
    std::map<CStatesDeclaration*,StateVariable> m_statesAlt;
    std::map<std::string, CHandlerDeclaration*> m_handlers;
    std::map<std::string, CMappingDeclaration*> m_mappings;

    CompilerOptions opts;

    void generateCode(CBlock& root,CompilerOptions &opts);
   // GenericValue runCode();
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
    const std::stack<const CIdentifier*>& getIdentStack() { return identifierStack; }
    const CIdentifier* currentIdent() { if (identifierStack.empty()) return NULL; else return identifierStack.top(); }
    void pushIdent(const CIdentifier* id) { identifierStack.push(id); }
    void popIdent() { identifierStack.pop(); }
};
