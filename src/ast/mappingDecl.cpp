#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Value.h>

extern void PrintErrorDuplication(const YYLTYPE &location, const YYLTYPE &originalLocation, const char *errorstring, ...);		// Todo refactor away

void CMappingDeclaration::prePass(CodeGenContext& context)
{

}

llvm::Value* CMappingDeclaration::codeGen(CodeGenContext& context)
{
	if (context.m_mappings.find(ident.name) != context.m_mappings.end())
	{
		PrintErrorDuplication(ident.nameLoc, context.m_mappings[ident.name]->ident.nameLoc, "Multiple declaration for mapping");
		context.errorFlagged = true;
	}
	else
	{
		context.m_mappings[ident.name] = this;
	}
	return nullptr;
}
