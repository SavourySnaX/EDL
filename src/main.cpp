#include <fstream>
#include <string>
#include <iostream>
#include <stdio.h>
#include <cstdarg>

#include "yyltype.h"
#include "generator.h"
#include "ast.h"
using namespace std;

extern int yyparse();
extern CBlock* g_ProgramBlock;

#define EDL_LANGUAGE_VERSION		"0.5"
#define EDL_COMPILER_VERSION		"2.0"

int Usage()
{
	cerr << "Usage: edl [opts] inputfile" << endl;
	cerr << "Compile edl into llvm source (to stdout) / native object (-o outputobject)" <<endl <<endl;
	cerr << "-s symbol      prepends symbol to all externally accessable symbols" << endl;
	cerr << "-o output      emits a native objectfile instead of llvm assembly" << endl;
	cerr << "-g             generate debug information" << endl;
	cerr << "-O0            disables optimisations" << endl;
	cerr << "-O1            enables all but experimental optimisations" << endl;
	cerr << "-O2            enables O1 plus experimental optimisations" << endl;
	cerr << "-t             trace on unimplemented instructions" << endl;
	cerr << "-n             disables disassembly generation" << endl;
	cerr << "-v             print version" << endl;
	cerr << "-h --help      print usage" << endl;

	return 0;
}

// FLEX support functions and globals - used to improve the error reporting
///////////////////////////////////////////////////////////////////////////
const int contextLines = 3;
static bool eof = false;
static int nRow = 0;
static int nBuffer = 0;
static int lBuffer = 0;
static int nTokenStart = 0;
static int nTokenLength = 0;
static int nTokenNextStart = 0;
std::ifstream inputFile;
std::string buffer;
std::string currentFileName;
std::map<std::string, std::vector<std::string> > fileLineMap;

// Need to buffer inputs in order to be able to back refer to them when error reporting - simple way, stick a map of filenames(paths) and lines in the file.

int resetFileInput(const char* filename)
{
	eof = false;
	nRow = 0;
	nBuffer = lBuffer = 0;

	currentFileName = filename;

	if (inputFile.is_open())
	{
		inputFile.close();
	}
	
	inputFile.open(currentFileName,std::ifstream::in);
	if (!inputFile.good())
	{
		return 1;
	}

	g_ProgramBlock=0;

	return 0;
}

static int getNextLine(void)
{
	int i;
	char *p;
  
	nBuffer = 0;
	nTokenStart = -1;
	nTokenNextStart = 0;
	eof = false;

	std::getline(inputFile, buffer);
	if (inputFile.eof())
	{
		eof = true;
		return 1;
	}
	if (inputFile.fail())
	{
		return -1;
	}

	buffer += "\n";
	std::replace(buffer.begin(), buffer.end(), '\t', ' ');

	fileLineMap[currentFileName].push_back(buffer);
	nRow += 1;
	lBuffer = buffer.length();

	return 0;
}

int GetNextChar(char *b, int maxBuffer) 
{
	int frc;

	if (eof)
		return 0;

	while (nBuffer >= lBuffer) 
	{
		frc = getNextLine();
		if (frc != 0)
			return 0;
	}

	b[0] = buffer[nBuffer];
	nBuffer += 1;

	return b[0] == 0 ? 0 : 1;
}
extern YYLTYPE yylloc;
void BeginToken(const char *t) 
{
	nTokenStart = nTokenNextStart;
	nTokenLength = strlen(t);
	nTokenNextStart = nBuffer; // + 1;

	yylloc.first_line = nRow;
	yylloc.first_column = nTokenStart;
	yylloc.last_line = nRow;
	yylloc.last_column = nTokenStart + nTokenLength - 1;
	yylloc.filename = currentFileName;
}

YYLTYPE CombineTokenLocations(const YYLTYPE &a, const YYLTYPE &b)
{
	YYLTYPE t;

	assert(a.filename == b.filename);

	t.first_line = a.first_line < b.first_line ? a.first_line : b.first_line;
	t.last_line = a.last_line > b.last_line ? a.last_line : b.last_line;
	t.first_column = a.first_column < b.first_column ? a.first_column : b.first_column;
	t.last_column = a.last_column > b.last_column ? a.last_column : b.last_column;
	t.filename = a.filename;

	return t;
}

void SkipChar()
{
	nTokenStart = nTokenNextStart;
	nTokenLength = 1;
	nTokenNextStart = nBuffer;
}

void PrintDetailedError(const char* errmsg,int start,int end,int srow,int erow, const char* filename)
{
	int i;

	fprintf(stderr, "Error: %s [%s:%d]\n", errmsg,filename, srow);

	for (int a = 0; a < contextLines; a++)
	{
		int pRow = srow - (contextLines - a);
		if (pRow >= 1)
		{
			fprintf(stderr, "%6d |%s", pRow, fileLineMap[filename][pRow-1].c_str());
		}
	}
	for (int b = srow; b <= erow; b++)
	{
		if (b>=1)
		{
			fprintf(stderr, "%6d |%s", b, fileLineMap[filename][b - 1].c_str());
			{
				fprintf(stderr, "       !");
				for (i = 0; i < start; i++)
					fprintf(stderr, " ");
				for (i = start; i <= end; i++)
					fprintf(stderr, "^");
				fprintf(stderr, "\n");
			}
		}
	}
}

void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...)
{
	static char errmsg[10000];
	va_list args;

	int start = 0;
	int end = fileLineMap[location.filename][location.first_line-1].length();

	va_start(args, errorstring);
	vsprintf(errmsg, errorstring, args);
	va_end(args);

	PrintDetailedError(errmsg, start, end,location.first_line,location.last_line,location.filename.c_str());
}

void PrintErrorDuplication(const YYLTYPE &location, const YYLTYPE &originalLocation, const char *errorstring, ...)
{
	static char errmsg[10000];
	va_list args;

	int start = location.first_column;
	int end = location.last_column;

	va_start(args, errorstring);
	vsprintf(errmsg, errorstring, args);
	va_end(args);

	PrintDetailedError(errmsg, start, end,location.first_line,location.last_line,location.filename.c_str());
	PrintDetailedError("duplicated here:", originalLocation.first_column, originalLocation.last_column, originalLocation.first_line, originalLocation.last_line, originalLocation.filename.c_str());
}


void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...)
{
	static char errmsg[10000];
	va_list args;

	int start = location.first_column;
	int end = location.last_column;

	va_start(args, errorstring);
	vsprintf(errmsg, errorstring, args);
	va_end(args);

	PrintDetailedError(errmsg, start, end,location.first_line,location.last_line,location.filename.c_str());
}

void PrintError(const char *errorstring, ...) 
{
	static char errmsg[10000];
	va_list args;

	int start = nTokenStart;
	int end = start + nTokenLength - 1;

	va_start(args, errorstring);
	vsprintf(errmsg, errorstring, args);
	va_end(args);

	PrintDetailedError(errmsg, start, end,nRow,nRow,currentFileName.c_str());

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
			if (strcmp(argv[a], "-g") == 0)
			{
				opts.generateDebug = 1;
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
			if (opts.inputFile==nullptr)
			{
				opts.inputFile=argv[a];
			}
			else
			{
				return Usage();
			}
		}
	}

	if (opts.inputFile == nullptr)
	{
		return Usage();
	}

	if (resetFileInput(opts.inputFile) != 0)
	{
		cerr << "Unable to open " << opts.inputFile << " for reading." << endl;
		return 2;
	}
	
	InitializeAllTargetInfos();
	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmParsers();
	InitializeAllAsmPrinters();

	int lexerError=yyparse();
	if (lexerError || g_ProgramBlock==0)
	{
		cerr << "Error : Unable to parse input" << endl;
		return 1;
	}
	
	GlobalContext globalContext(opts);
	CodeGenContext rootContext(globalContext,nullptr);
	rootContext.generateCode(*g_ProgramBlock);

	if (rootContext.errorFlagged)
	{
		std::cerr << "Compilation Failed" << std::endl;
		return 2;
	}
	
	return 0;
}

