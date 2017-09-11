#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

void COperandIdent::DeclareLocal(CodeGenContext& context, unsigned num)
{
	BitVariable newLocal(size.getAPInt(), num);

	newLocal.value = new llvm::AllocaInst(context.getIntType(size), 0, ident.name.c_str(), context.currentBlock());

	llvm::ConstantInt* initialConstant = context.getConstantInt(newLocal.cnst);
	llvm::StoreInst* stor = new llvm::StoreInst(initialConstant, newLocal.value, false, context.currentBlock());

	context.locals()[ident.name] = newLocal;
}
