#include <iostream>
#include <stdio.h>
#include "generator.h"
#include "ast.h"

using namespace std;

extern int yyparse();
extern CBlock* g_ProgramBlock;
extern FILE *yyin;

#define EDL_LANGUAGE_VERSION		"0.4"
#define EDL_COMPILER_VERSION		"1.1"

int Usage()
{
	cerr << "Usage: edl [opts] [inputfile]" << endl;
	cerr << "Compile edl into llvm source (default reads/writes from/to stdout)" <<endl <<endl;
	cerr << "-s symbol		prepends symbol to all externally accessable symbols" << endl;
	cerr << "-o output		emits a native objectfile instead of llvm assembly" << endl;
	cerr << "-O0			disables optimisations" << endl;
	cerr << "-O1			enables all but experimental optimisations" << endl;
	cerr << "-O2			enables O1 plus experimental optimisations" << endl;
	cerr << "-t                     trace on unimplemented instructions" << endl;
	cerr << "-n			disables disassembly generation" << endl;
	cerr << "-v			print version" << endl;
	cerr << "-h --help		print usage" << endl;

	return 0;
}


int main(int argc, char **argv)
{
	CompilerOptions	opts;

	for (int a=1;a<argc;a++)
	{
		if (argv[a][0]=='-')
		{
			if (strcmp(argv[a],"-o")==0)
			{
				if ((a+1)<argc)
				{
					opts.outputFile=argv[a+1];
				}
				else
				{
					return Usage();
				}
				a+=1;
				continue;
			}
			if (strcmp(argv[a],"-s")==0)
			{
				if ((a+1)<argc)
				{
					opts.symbolModifier=argv[a+1];
				}
				else
				{
					return Usage();
				}
				a+=1;
				continue;
			}
			if (strcmp(argv[a],"-t")==0)
			{
				opts.traceUnimplemented=1;
			}
			if (strcmp(argv[a],"-n")==0)
			{
				opts.generateDisassembly=0;
			}
			if (strcmp(argv[a],"-v")==0)
			{
				cout << "edl.exe "<<EDL_COMPILER_VERSION<<":"<<EDL_LANGUAGE_VERSION<<endl;
				return 0;
			}
			if (strcmp(argv[a],"-h")==0 || strcmp(argv[a],"--help")==0)
			{
				return Usage();
			}
			if (strncmp(argv[a],"-O",2)==0)
			{
				if (strlen(argv[a])==3)
				{
					opts.optimisationLevel=argv[a][2]-'0';
				}
				else
				{
					return Usage();
				}
			}
		}
		else
		{
			if (opts.inputFile==NULL)
			{
				opts.inputFile=argv[a];
			}
			else
			{
				return Usage();
			}
		}
	}

	if (opts.inputFile)
	{
		yyin = fopen(opts.inputFile,"r");
		if (!yyin)
		{
			cerr << "Unable to open " << opts.inputFile << " for reading." << endl;
			return 2;
		}

	}
	
	InitializeAllTargetInfos();
	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmParsers();
	InitializeAllAsmPrinters();

	yyparse();
	if (g_ProgramBlock==0)
	{
		cerr << "Error : Unable to parse input" << endl;
		return 1;
	}
	
	CodeGenContext rootContext(NULL);
	rootContext.generateCode(*g_ProgramBlock,opts);

	if (rootContext.errorFlagged)
	{
		return 2;
	}
	
	return 0;
}

