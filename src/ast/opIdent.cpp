#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

void COperandIdent::DeclareLocal(CodeGenContext& context, unsigned num)
{
	BitVariable newLocal(size.getAPInt(), num);

	context.gContext.irBuilder->SetInsertPoint(context.currentBlock());
	if (context.gContext.opts.generateDebug)
	{
		context.gContext.irBuilder->SetCurrentDebugLocation(llvm::DebugLoc::get(ident.nameLoc.first_line, ident.nameLoc.first_column, context.gContext.scopingStack.top()));
	}

	newLocal.value = context.gContext.irBuilder->CreateAlloca(context.getIntType(size), 0, nullptr, ident.name.c_str());

	if (context.gContext.opts.generateDebug)
	{
		std::string name = "Bit" + std::to_string(context.getIntType(size)->getIntegerBitWidth());
		llvm::DIType* dbgType = context.gContext.dbgBuilder->createBasicType(name, context.getIntType(size)->getIntegerBitWidth(), 0);

		llvm::DILocalVariable* D = context.gContext.dbgBuilder->createAutoVariable(context.gContext.scopingStack.top(), ident.name, context.gContext.CreateNewDbgFile(ident.nameLoc.filename.c_str()), ident.nameLoc.first_line, dbgType);
		context.gContext.dbgBuilder->insertDeclare(newLocal.value, D, context.gContext.dbgBuilder->createExpression(), llvm::DebugLoc::get(ident.nameLoc.first_line, ident.nameLoc.first_column, context.gContext.scopingStack.top()), context.gContext.irBuilder->GetInsertBlock());
	}
	llvm::ConstantInt* initialConstant = context.getConstantInt(newLocal.cnst);
	context.gContext.irBuilder->CreateStore(initialConstant, newLocal.value);

	context.locals()[ident.name] = newLocal;
}
