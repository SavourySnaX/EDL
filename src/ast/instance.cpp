#include <stdio.h>

#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Value.h>

extern int yyparse();
extern CBlock* g_ProgramBlock;
extern int resetFileInput(const char* filename);

void CInstance::prePass(CodeGenContext& context)
{
	// On the prepass, we need to generate the code for this module
	std::string includeName = filename.quoted.substr(1, filename.quoted.length() - 2);

	if (resetFileInput(includeName.c_str()) != 0)
	{
		context.gContext.ReportError(nullptr, EC_ErrorAtLocation, filename.quotedLoc, "Unable to instance module %s", includeName.c_str());
		return;
	}
	yyparse();
	if (g_ProgramBlock == 0)
	{
		context.gContext.ReportError(nullptr, EC_ErrorAtLocation, filename.quotedLoc, "Unable to parse module %s", includeName.c_str());
		return;
	}

	CodeGenContext* includefile;

	includefile = new CodeGenContext(context.gContext, &context);
	includefile->moduleName = ident.name + ".";

	if (context.gContext.opts.generateDebug)
	{
		context.gContext.scopingStack.push(context.gContext.CreateNewDbgFile(includeName.c_str()));
	}

	includefile->generateCode(*g_ProgramBlock);

	if (context.gContext.opts.generateDebug)
	{
		context.gContext.scopingStack.pop();
	}
	if (includefile->isErrorFlagged())
	{
		context.gContext.ReportError(nullptr , EC_ErrorAtLocation, filename.quotedLoc, "Unable to parse module %s", includeName.c_str());
		return;
	}
	context.m_includes[ident.name + "."] = includefile;
}

llvm::Value* CInstance::codeGen(CodeGenContext& context)
{
	// At present doesn't generate any code - maybe never will
	return nullptr;
}
