#pragma once

#include <map>
#include <stack>
#include <vector>

#include <llvm/IR/Instructions.h>

#include "nodes.h"
#include "ast/integer.h"

class CBlock;
class CStatesDeclaration;
class CHandlerDeclaration;
class CMappingDeclaration;
class CIdentifier;
class CodeGenContext;
class CAffect;
class CExecute;

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

class ExecuteSaved
{
public:
	llvm::Function* func;
	std::vector<llvm::Value*> args;
	std::string name;
	llvm::APInt caseNum;
};

class ExecuteInformation
{
public:
	llvm::BasicBlock* blockEndForExecute;
	llvm::SwitchInst* switchForExecute;
	CExecute* execute;
	YYLTYPE executeLoc;

	std::stack<ExecuteSaved> processLater;
};

class CodeGenBlock 
{
public:
	llvm::BasicBlock *block;
	std::map<std::string, BitVariable> locals;
};

#include "globalContext.h"

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
	void pushBlock(llvm::BasicBlock *block)
	{
		std::map<std::string, BitVariable> tLocals;
		if (blocks.size() > 0)
			tLocals = blocks.top()->locals;
		blocks.push(new CodeGenBlock());
		blocks.top()->block = block;
		blocks.top()->locals = tLocals;
	}
	void pushBlock(llvm::BasicBlock *block, const YYLTYPE& blockStartLocation)
	{
		pushBlock(block);
		if (gContext.opts.generateDebug)
		{
			gContext.scopingStack.push(gContext.dbgBuilder->createLexicalBlock(gContext.scopingStack.top(), gContext.scopingStack.top()->getFile(), blockStartLocation.first_line, blockStartLocation.first_column));
		}
	}
	void popBlock()
	{
		CodeGenBlock *top = blocks.top();
		blocks.pop();
		delete top;
	}
	void popBlock(const YYLTYPE& blockEndLocation)
	{
		if (gContext.opts.generateDebug)
		{
			gContext.scopingStack.pop();
		}
		popBlock();
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

	void StartFunctionDebugInfo(llvm::Function* func, const YYLTYPE& declLoc);
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
	llvm::BasicBlock*		makeBasicBlock(const llvm::Twine& name, llvm::Function* parent, llvm::BasicBlock* insertBefore = nullptr) { return gContext.makeBasicBlock(name, parent, insertBefore); }
	llvm::ReturnInst*		makeReturn(llvm::BasicBlock* insertAtEnd) { return gContext.makeReturn(insertAtEnd); }
	llvm::ReturnInst*		makeReturnValue(llvm::Value* value, llvm::BasicBlock* insertAtEnd) { return gContext.makeReturnValue(value, insertAtEnd); }
	llvm::Function*			makeFunction(llvm::FunctionType* fType, llvm::Function::LinkageTypes fLinkType, const llvm::Twine& fName) { return gContext.makeFunction(fType, fLinkType, fName); }
	llvm::GlobalVariable*	makeGlobal(llvm::Type* gType, bool isConstant, llvm::GlobalVariable::LinkageTypes gLinkType, llvm::Constant* gInitialiser, const llvm::Twine& gName) { return gContext.makeGlobal(gType, isConstant, gLinkType, gInitialiser, gName); }
};
