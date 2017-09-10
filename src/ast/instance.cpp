#include <stdio.h>

#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away
extern llvm::DIFile* CreateNewDbgFile(const char* filepath, llvm::DIBuilder* dbgBuilder);		// Todo refactor away

extern int yyparse();
extern CBlock* g_ProgramBlock;
extern int resetFileInput(const char* filename);

void CInstance::prePass(CodeGenContext& context)
{
	// On the prepass, we need to generate the code for this module
	std::string includeName = filename.quoted.substr(1, filename.quoted.length() - 2);

	if (resetFileInput(includeName.c_str()) != 0)
	{
		PrintErrorFromLocation(filename.quotedLoc, "Unable to instance module %s", includeName.c_str());
		context.errorFlagged = true;
		return;
	}
	yyparse();
	if (g_ProgramBlock == 0)
	{
		PrintErrorFromLocation(filename.quotedLoc, "Unable to parse module %s", includeName.c_str());
		context.errorFlagged = true;
		return;
	}

	CodeGenContext* includefile;

	includefile = new CodeGenContext(context.gContext, &context);
	includefile->moduleName = ident.name + ".";

	if (context.gContext.opts.generateDebug)
	{
		context.gContext.scopingStack.push(CreateNewDbgFile(includeName.c_str(), includefile->dbgBuilder));
	}

	includefile->generateCode(*g_ProgramBlock);

	if (context.gContext.opts.generateDebug)
	{
		context.gContext.scopingStack.pop();
	}
	if (includefile->errorFlagged)
	{
		PrintErrorFromLocation(filename.quotedLoc, "Unable to parse module %s", includeName.c_str());
		context.errorFlagged = true;
		return;
	}
	context.m_includes[ident.name + "."] = includefile;
}

llvm::Value* CInstance::codeGen(CodeGenContext& context)
{
	// At present doesn't generate any code - maybe never will
	return nullptr;
}
