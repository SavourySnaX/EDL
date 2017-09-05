#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/StringRef.h>

void CString::prePass(CodeGenContext& context)
{
}

llvm::Value* CString::codeGen(CodeGenContext& context)
{
	// Create a global variable to point to the string data
	llvm::ArrayType* arrayType = llvm::ArrayType::get(llvm::IntegerType::get(TheContext, 8), quoted.length() - 1);
	llvm::GlobalVariable* globalString = new llvm::GlobalVariable(*context.module, arrayType, true, llvm::GlobalValue::PrivateLinkage, 0, context.symbolPrepend + ".str");
	globalString->setAlignment(1);

	// Initial the global variable with the contents of the string (minus the quotation marks)
	llvm::Constant* constString = llvm::ConstantDataArray::getString(TheContext, quoted.substr(1, quoted.length() - 2), true);
	globalString->setInitializer(constString);

	// Finally return a ptr to the start of the string (using GEP)
	std::vector<llvm::Constant*> gepIndices;
	llvm::ConstantInt* constInt64Zero = llvm::ConstantInt::get(TheContext, llvm::APInt(64, llvm::StringRef("0"), 10));
	gepIndices.push_back(constInt64Zero);
	gepIndices.push_back(constInt64Zero);
	return llvm::ConstantExpr::getGetElementPtr(nullptr, globalString, gepIndices);
}

