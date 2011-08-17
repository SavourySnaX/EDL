#include <iostream>
#include <stdio.h>
#include "generator.h"
#include "ast.h"

using namespace std;

extern int yyparse();
extern CBlock* g_ProgramBlock;
extern FILE *yyin;
CodeGenContext* globalContext;

bool JustCompiledOutput=true;

int main(int argc, char **argv)
{
	if (argc == 2)
	{
		yyin = fopen(argv[1],"r");
	}

	if (argc == 3)
	{
		yyin = fopen(argv[1],"r");
		JustCompiledOutput=false;
	}

	yyparse();
	if (g_ProgramBlock==0)
	{
		cerr << "Error : Unable to parse input" << endl;
		return 1;
	}
	InitializeNativeTarget();
	CodeGenContext context;
	globalContext = &context;
	context.generateCode(*g_ProgramBlock);
	context.runCode();

	if (context.errorFlagged)
	{
		return 2;
	}
	
	return 0;
}

