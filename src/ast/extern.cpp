#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

void CExternDecl::prePass(CodeGenContext& context)
{

}

llvm::Value* CExternDecl::codeGen(CodeGenContext& context)
{
	// Build up the function type parameters. At present the only allowed sizes are : 
	// 	0 -> void	(only allowed for return!!)
	// 	8 -> unsigned char  (technically u_int8_t)
	// 	16-> unsigned short (technically u_int16_t)
	// 	32-> unsigned int   (technically u_int32_t)

	std::vector<llvm::Type*> FuncTy_8_args;
	for (int a = 0; a < params.size(); a++)
	{
		unsigned size = params[a]->getAPInt().getLimitedValue();
		if (size != 8 && size != 16 && size != 32)
		{
			PrintErrorFromLocation(params[a]->getSourceLocation(), "External C functions must use C size parameters (8,16 or 32 bits)");
			context.FlagError();
			return nullptr;
		}
		FuncTy_8_args.push_back(context.getIntType(size));
	}
	llvm::FunctionType* FuncTy_8;
	if (returns.empty())
	{
		FuncTy_8 = llvm::FunctionType::get(context.getVoidType(), FuncTy_8_args, false);
	}
	else
	{
		unsigned size = returns[0]->getAPInt().getLimitedValue();
		if (size != 8 && size != 16 && size != 32)
		{
			PrintErrorFromLocation(returns[0]->getSourceLocation(), "External C functions must use C size parameters (8,16 or 32 bits)");
			context.FlagError();
			return nullptr;
		}
		FuncTy_8 = llvm::FunctionType::get(context.getIntType(size), FuncTy_8_args, false);
	}

	context.m_externFunctions[name.name] = context.makeFunction(FuncTy_8, llvm::GlobalValue::ExternalLinkage, context.getSymbolPrefix()+name.name);

	return nullptr;
}
