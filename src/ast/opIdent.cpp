#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

void COperandIdent::DeclareLocal(CodeGenContext& context, unsigned num)
{
	BitVariable temp(size.getAPInt(),0);

	temp.value = arg;
	temp.value->setName(ident.name);
	context.locals()[ident.name] = temp;
}
	
void COperandIdent::GetArgTypes(CodeGenContext& context, std::vector<llvm::Type*>& argVector) const
{
	argVector.push_back(context.getIntType(size.getAPInt().getLimitedValue()));
}

void COperandIdent::GetArgs(CodeGenContext& context, std::vector<llvm::Value*>& argVector,unsigned num) const
{
	argVector.push_back(context.getConstantInt(GetComputableConstant(context,num)));
}
