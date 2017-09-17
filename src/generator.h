#pragma once

#include <stack>
#include <typeinfo>
#include <stdarg.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/IR/DataLayout.h>
//#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
//#include <llvm/ModuleProvider.h>
#include <llvm/Support/TargetSelect.h>
//#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/raw_ostream.h>

#include "nodes.h"
#include "ast/integer.h"

class CBlock;
class CStatesDeclaration;
class CHandlerDeclaration;
class CMappingDeclaration;
class CIdentifier;
class CodeGenContext;
class CExpression;
class CAffect;

typedef std::vector<CAffect*> AffectorList;

#include "bitvariable.h"

class StateVariable
{
public:
	llvm::Value* currentState;
	llvm::Value* nextState;
	llvm::Value* stateStackNext;
	llvm::Value* stateStackIndex;
	CStatesDeclaration* decl;
};

class ExecuteInformation
{
public:
    llvm::BasicBlock* blockEndForExecute;
    llvm::SwitchInst* switchForExecute;
	YYLTYPE executeLoc;
};

class CodeGenBlock 
{
public:
    llvm::BasicBlock *block;
    std::map<std::string, BitVariable> locals;
};

class CompilerOptions
{
public:

	CompilerOptions() { symbolModifier = nullptr; inputFile = nullptr; outputFile = nullptr; optimisationLevel = 0; traceUnimplemented = 0; generateDisassembly = 1; generateDebug = 0; }
	
	char *symbolModifier;
	char *inputFile;
	char *outputFile;
	int optimisationLevel;
	int traceUnimplemented;
	int generateDisassembly;
	int generateDebug;
};

enum ErrorClassification
{
	EC_InternalError,
	EC_ErrorWholeLine,
	EC_ErrorAtLocation
};

void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, va_list args);
void PrintErrorDuplication(const YYLTYPE &location, const YYLTYPE &originalLocation, const char *errorstring, va_list args);
void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, va_list args);

class GlobalContext
{
private:
	llvm::LLVMContext			llvmContext;
	bool						InitGlobalCodeGen();
	bool						FinaliseCodeGen(CodeGenContext& rootContext);
	void						SetupOptimisationPasses(llvm::legacy::PassManager& pm,CodeGenContext& rootContext);
	
public:
	GlobalContext(CompilerOptions& options) : opts(options) 
	{  
		errorFlagged = false;
	}

	std::map<std::string, bool>					impedanceRequired;
	std::stack<llvm::DIScope*>					scopingStack;
	std::map<llvm::Function*,llvm::Function*>	connectFunctions;	
	std::string									symbolPrefix;
	CompilerOptions&							opts;
	llvm::DIBuilder*							dbgBuilder;
	llvm::DICompileUnit*						compileUnit;
	llvm::ExecutionEngine*						llvmExecutionEngine;
	llvm::Module*								llvmModule;
	llvm::Function*								debugTraceChar;
	llvm::Function*								debugTraceMissing;
	llvm::Function*								debugBusTap;
	bool										errorFlagged;

	bool generateCode(CBlock& root);

	template<class T>
	T ReportError(T retVal, ErrorClassification eType, const YYLTYPE& location, const char* formatString, ...)
	{
		va_list args;
		va_start(args, formatString);
		switch (eType)
		{
		case EC_InternalError:
			PrintErrorWholeLine(location, formatString, args );
			assert(0);
			break;
		case EC_ErrorWholeLine:
			PrintErrorWholeLine(location, formatString, args );
			break;
		case EC_ErrorAtLocation:
			PrintErrorFromLocation(location, formatString, args );
			break;
		}
		va_end(args);
		errorFlagged = true;
		return retVal;
	}

	template<class T>
	T ReportDuplicationError(T retVal, const YYLTYPE& location, const YYLTYPE& origLocation, const char* formatString, ...)
	{
		va_list args;
		va_start(args, formatString);
		PrintErrorDuplication(location, origLocation, formatString, args );
		va_end(args);
		errorFlagged = true;
		return retVal;
	}

	llvm::Value* ReportUndefinedStateError(StateIdentList &stateIdents);

	llvm::DIFile* CreateNewDbgFile(const char* filepath);
	llvm::LLVMContext& getLLVMContext() { return llvmContext; }
	
	// Type Wrappers
	llvm::Type* getVoidType() { return llvm::Type::getVoidTy(llvmContext); }
	llvm::Type* getIntType(unsigned size) { return llvm::IntegerType::get(llvmContext, size); }
	llvm::Type* getIntType(const CInteger& size) { return llvm::IntegerType::get(llvmContext, size.getAPInt().getLimitedValue()); }
	llvm::Type* getIntType(const llvm::APInt& size) { return llvm::IntegerType::get(llvmContext, size.getLimitedValue()); }

	// Constant Wrappers
	llvm::ConstantInt*	getConstantInt(const llvm::APInt& value) { return llvm::ConstantInt::get(llvmContext, value); }
	llvm::ConstantInt*	getConstantZero(unsigned numBits) { return llvm::ConstantInt::get(llvmContext, llvm::APInt(numBits,llvm::StringRef("0"),10)); }
	llvm::ConstantInt*	getConstantOnes(unsigned numBits) { return llvm::ConstantInt::get(llvmContext, ~llvm::APInt(numBits,llvm::StringRef("0"),10)); }
	llvm::Constant*		getString(const std::string& withQuotes) { return llvm::ConstantDataArray::getString(llvmContext, withQuotes.substr(1, withQuotes.length() - 2), true); }

	// Function Wrappers
	llvm::BasicBlock*		makeBasicBlock(const llvm::Twine&name, llvm::Function* parent = nullptr, llvm::BasicBlock* insertBefore = nullptr) { return llvm::BasicBlock::Create(llvmContext, name, parent, insertBefore); }
	llvm::ReturnInst*		makeReturn(llvm::BasicBlock* insertAtEnd) { return llvm::ReturnInst::Create(llvmContext, insertAtEnd); }
	llvm::ReturnInst*		makeReturnValue(llvm::Value* value, llvm::BasicBlock* insertAtEnd) { return llvm::ReturnInst::Create(llvmContext, value, insertAtEnd); }
	llvm::Function*			makeFunction(llvm::FunctionType* fType, llvm::Function::LinkageTypes fLinkType, const llvm::Twine& fName);
	llvm::GlobalVariable*	makeGlobal(llvm::Type* gType, bool isConstant, llvm::GlobalVariable::LinkageTypes gLinkType, llvm::Constant* gInitialiser, const llvm::Twine& gName);
};

class CodeGenContext
{
	std::stack<CodeGenBlock *> blocks;
	std::stack<CStatesDeclaration *> statesStack;
	std::stack<const CIdentifier *> identifierStack;
	llvm::Function *mainFunction;

	void GenerateDisassmTables();

public:
	GlobalContext& gContext;

	std::string stateLabelStack;

	CHandlerDeclaration* parentHandler;

	bool isRoot;
	CodeGenContext(GlobalContext& globalContext, CodeGenContext* parent);

	std::string moduleName;

	struct myAPIntCompare
	{
		bool operator()(const llvm::APInt& s1, const llvm::APInt& s2) const
		{
			llvm::APInt f1 = s1;
			llvm::APInt f2 = s2;

			if (f1.getBitWidth() != f2.getBitWidth())
			{
				if (f1.getBitWidth() < f2.getBitWidth())
				{
					f1 = f1.zext(f2.getBitWidth());
				}
				else
				{
					f2 = f2.zext(f1.getBitWidth());
				}
			}
			return f1.ult(f2);
		}
	};


	llvm::Function* LookupFunctionInExternalModule(const std::string& module, const std::string& name);
	bool LookupBitVariable(BitVariable& outVar, const std::string& module, const std::string& name, const YYLTYPE &modLoc, const YYLTYPE &nameLoc);

	// Local to current file
	std::map<std::string, CodeGenContext*> m_includes;
	std::map<std::string, BitVariable> m_globals;
	std::map<std::string, llvm::Function*> m_externFunctions;
	std::map<std::string, StateVariable> m_states;
	std::map<CStatesDeclaration*, StateVariable> m_statesAlt;
	std::map<std::string, CHandlerDeclaration*> m_handlers;
	std::map<std::string, CMappingDeclaration*> m_mappings;
	std::map<std::string, std::map<llvm::APInt, std::string, myAPIntCompare> >     disassemblyTable;
	std::map<std::string, std::vector<ExecuteInformation> > executeLocations;
	AffectorList	curAffectors;


	void generateCode(CBlock& root);
	std::map<std::string, BitVariable>& locals() { return blocks.top()->locals; }
	std::map<std::string, BitVariable>& globals() { return m_globals; }
	std::map<std::string, StateVariable>& states() { return m_states; }
	std::map<CStatesDeclaration*, StateVariable>& statesAlt() { return m_statesAlt; }
	llvm::BasicBlock *currentBlock() { if (blocks.size() > 0) return blocks.top()->block; else return nullptr; }
	void setBlock(llvm::BasicBlock *block) { blocks.top()->block = block; }
	void pushBlock(llvm::BasicBlock *block, YYLTYPE& blockStartLocation)
	{
		std::map<std::string, BitVariable> tLocals;
		if (blocks.size() > 0)
			tLocals = blocks.top()->locals;
		blocks.push(new CodeGenBlock());
		blocks.top()->block = block;
		blocks.top()->locals = tLocals;
		if (gContext.opts.generateDebug)
		{
			gContext.scopingStack.push(gContext.dbgBuilder->createLexicalBlock(gContext.scopingStack.top(), gContext.scopingStack.top()->getFile(), blockStartLocation.first_line, blockStartLocation.first_column));
		}
	}
	void popBlock(YYLTYPE& blockEndLocation)
	{
		CodeGenBlock *top = blocks.top();
		if (gContext.opts.generateDebug)
		{
			gContext.scopingStack.pop();
		}
		blocks.pop();
		delete top;
	}
	bool isStateEmpty()
	{
		return statesStack.empty();
	}
	CStatesDeclaration* currentState()
	{
		if (statesStack.empty()) 
			return nullptr; 
		return statesStack.top();
	}
	void pushState(CStatesDeclaration *state) { statesStack.push(state); }
	void popState() { statesStack.pop(); }
	const std::stack<const CIdentifier*>& getIdentStack() { return identifierStack; }
	const CIdentifier* currentIdent() { if (identifierStack.empty()) return nullptr; else return identifierStack.top(); }
	void pushIdent(const CIdentifier* id) { identifierStack.push(id); }
	void popIdent() { identifierStack.pop(); }

	void StartFunctionDebugInfo(llvm::Function* func, YYLTYPE& declLoc);
	void EndFunctionDebugInfo();

	// Global state accessors
	const std::string& getSymbolPrefix() const { return gContext.symbolPrefix; }
	void FlagError() { gContext.errorFlagged = true; }
	bool isErrorFlagged() const { return gContext.errorFlagged; }

	// Type Wrappers
	llvm::Type* getVoidType() const { return gContext.getVoidType(); }
	llvm::Type* getIntType(unsigned size) const { return gContext.getIntType(size); }
	llvm::Type* getIntType(const CInteger& size) const { return gContext.getIntType(size); }
	llvm::Type* getIntType(const llvm::APInt& size) const { return gContext.getIntType(size); }

	// Constant Wrappers
	llvm::ConstantInt*	getConstantInt(const llvm::APInt& value) { return gContext.getConstantInt(value); }
	llvm::ConstantInt*	getConstantZero(unsigned numBits) { return gContext.getConstantZero(numBits); }
	llvm::ConstantInt*	getConstantOnes(unsigned numBits) { return gContext.getConstantOnes(numBits); }
	llvm::Constant*		getString(const std::string& withQuotes) { return gContext.getString(withQuotes); }

	// Function Wrappers
	llvm::BasicBlock*		makeBasicBlock(const llvm::Twine& name, llvm::Function* parent = nullptr, llvm::BasicBlock* insertBefore = nullptr) { return gContext.makeBasicBlock(name, parent, insertBefore); }
	llvm::ReturnInst*		makeReturn(llvm::BasicBlock* insertAtEnd) { return gContext.makeReturn(insertAtEnd); }
	llvm::ReturnInst*		makeReturnValue(llvm::Value* value, llvm::BasicBlock* insertAtEnd) { return gContext.makeReturnValue(value, insertAtEnd); }
	llvm::Function*			makeFunction(llvm::FunctionType* fType, llvm::Function::LinkageTypes fLinkType, const llvm::Twine& fName) { return gContext.makeFunction(fType, fLinkType, fName); }
	llvm::GlobalVariable*	makeGlobal(llvm::Type* gType, bool isConstant, llvm::GlobalVariable::LinkageTypes gLinkType, llvm::Constant* gInitialiser, const llvm::Twine& gName) { return gContext.makeGlobal(gType, isConstant, gLinkType, gInitialiser, gName); }
};
