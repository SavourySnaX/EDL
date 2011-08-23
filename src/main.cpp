#include <iostream>
#include <stdio.h>
#include "generator.h"
#include "ast.h"

using namespace std;

extern int yyparse();
extern CBlock* g_ProgramBlock;
extern FILE *yyin;

bool JustCompiledOutput=true;

int main(int argc, char **argv)
{
	InitializeNativeTarget();
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
	
	CodeGenContext rootContext(NULL);
	rootContext.generateCode(*g_ProgramBlock);
	rootContext.runCode();

	if (rootContext.errorFlagged)
	{
		return 2;
	}
	
	return 0;
}

