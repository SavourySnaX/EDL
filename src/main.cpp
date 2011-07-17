#include <iostream>
#include "generator.h"
#include "ast.h"

using namespace std;

extern int yyparse();
extern CBlock* g_ProgramBlock;

CodeGenContext* globalContext;

int main(int argc, char **argv)
{
	yyparse();
	std::cout << g_ProgramBlock << endl;
 //       # see http://comments.gmane.org/gmane.comp.compilers.llvm.devel/33877
	InitializeNativeTarget();
	CodeGenContext context;
	globalContext = &context;
	context.generateCode(*g_ProgramBlock);
	context.runCode();
	
	return 0;
}

