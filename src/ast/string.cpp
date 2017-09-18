#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>

void CString::prePass(CodeGenContext& context)
{
}

llvm::Value* CString::codeGen(CodeGenContext& context)
{
	// Create a global variable to point to the string data
	llvm::Constant* constString = context.getString(quoted);
	llvm::ArrayType* arrayType = llvm::ArrayType::get(context.getIntType(8), quoted.length() - 1);
	llvm::GlobalVariable* globalString = context.makeGlobal(arrayType, true, llvm::GlobalValue::PrivateLinkage, constString, context.getSymbolPrefix() + ".str");
	globalString->setAlignment(1);

	// Finally return a ptr to the start of the string (using GEP)
	std::vector<llvm::Constant*> gepIndices;
	llvm::ConstantInt* constInt64Zero = context.getConstantZero(64);
	gepIndices.push_back(constInt64Zero);
	gepIndices.push_back(constInt64Zero);
	return llvm::ConstantExpr::getGetElementPtr(nullptr, globalString, gepIndices);
}

