#pragma once
#include <string>
#define YYLTYPE YYLTYPE
#define YYLTYPE_IS_DECLARED 1
	typedef struct YYLTYPE
	{
		int first_line;
		int first_column;
		int last_line;
		int last_column;
	    std::string filename;
	} YYLTYPE;

extern YYLTYPE CombineTokenLocations(const YYLTYPE &a, const YYLTYPE &b);

