#pragma once

#include <map>
#include <stack>
#include <stdarg.h>

#include "nodes.h"
#include "ast/integer.h"

#include <llvm/ADT/APInt.h>
#include <llvm/DebugInfo/DIContext.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>

class CBlock;
class CodeGenContext;

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
	bool						SetupPassesAndRun(CodeGenContext& rootContext);
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
	llvm::IRBuilder<>*							irBuilder;
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
	llvm::BasicBlock*		makeBasicBlock(const llvm::Twine&name, llvm::Function* parent, llvm::BasicBlock* insertBefore = nullptr) { return llvm::BasicBlock::Create(llvmContext, name, parent, insertBefore); }
	llvm::ReturnInst*		makeReturn(llvm::BasicBlock* insertAtEnd) { return llvm::ReturnInst::Create(llvmContext, insertAtEnd); }
	llvm::ReturnInst*		makeReturnValue(llvm::Value* value, llvm::BasicBlock* insertAtEnd) { return llvm::ReturnInst::Create(llvmContext, value, insertAtEnd); }
	llvm::Function*			makeFunction(llvm::FunctionType* fType, llvm::Function::LinkageTypes fLinkType, const llvm::Twine& fName);
	llvm::GlobalVariable*	makeGlobal(llvm::Type* gType, bool isConstant, llvm::GlobalVariable::LinkageTypes gLinkType, llvm::Constant* gInitialiser, const llvm::Twine& gName);
};
