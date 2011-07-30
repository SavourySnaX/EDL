#include <iostream>
#include <stdio.h>
#include "generator.h"
#include "ast.h"

using namespace std;

extern int yyparse();
extern CBlock* g_ProgramBlock;
extern FILE *yyin;
CodeGenContext* globalContext;

int main(int argc, char **argv)
{
	if (argc == 2)
	{
		yyin = fopen(argv[1],"r");
	}

	yyparse();
	InitializeNativeTarget();
	CodeGenContext context;
	globalContext = &context;
	context.generateCode(*g_ProgramBlock);
	context.runCode();
	
	return 0;
}

