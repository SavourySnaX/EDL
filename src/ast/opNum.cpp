#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

void COperandNumber::GetArgTypes(CodeGenContext& context, std::vector<llvm::Type*>& argVector) const
{
	// Constants don't need to be passed as arguments to the instruction function (they will have been dealt with already by the execute switch table)
}

void COperandNumber::GetArgs(CodeGenContext& context, std::vector<llvm::Value*>& argVector,unsigned num) const
{
	// Constants don't need to be passed as arguments to the instruction function (they will have been dealt with already by the execute switch table)
}
