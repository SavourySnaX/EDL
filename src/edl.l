%{
#include <string>
#include "ast.h"
#include "parser.hpp"
#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)

unsigned int g_CurLine=1;		/* For error handling */

extern "C" int yywrap() { }
%}

%%

DECLARE					{ return TOKEN(TOK_DECLARE); }
HANDLER					{ return TOKEN(TOK_HANDLER); }
STATES					{ return TOKEN(TOK_STATES); }
STATE					{ return TOKEN(TOK_STATE); }
ALIAS					{ return TOKEN(TOK_ALIAS); }
IF					{ return TOKEN(TOK_IF); }


DEBUG_TRACE				{ return TOKEN(TOK_TRACE); }
BASE					{ return TOKEN(TOK_BASE); }

[a-zA-Z_][a-zA-Z0-9_]*			{ SAVE_TOKEN; return TOK_IDENTIFIER; }
[0-9]+					{ SAVE_TOKEN; return TOK_INTEGER; /* decimal */}
\%[01]+					{ SAVE_TOKEN; return TOK_INTEGER; /* binary */}
\$[0-9a-fA-F]+				{ SAVE_TOKEN; return TOK_INTEGER; /* hexadecimal */}
\"(\\.|[^"])*\"				{ SAVE_TOKEN; return TOK_STRING; }

"=="					{ return TOKEN(TOK_CMPEQ); }
"{"					{ return TOKEN(TOK_LBRACE); }
"}"					{ return TOKEN(TOK_RBRACE); }
"["					{ return TOKEN(TOK_LSQR); }
"]"					{ return TOKEN(TOK_RSQR); }
":"					{ return TOKEN(TOK_COLON); }
";"					{ return TOKEN(TOK_EOS); }
","					{ return TOKEN(TOK_COMMA); }
"<-"					{ return TOKEN(TOK_ASSIGNLEFT); }
"->"					{ return TOKEN(TOK_ASSIGNRIGHT); }
"+"					{ return TOKEN(TOK_ADD); }
"-"					{ return TOKEN(TOK_SUB); }
"("					{ return TOKEN(TOK_OBR); }
")"					{ return TOKEN(TOK_CBR); }
"|"					{ return TOKEN(TOK_BAR); }

[ \t\r]					{ /* Consume white space */ }
#.*					{ /* Ignore comments */ }

[\n]					{ g_CurLine++; }

.					{ printf("Unrecognised character in input : %c  (line %d)\n",*yytext,g_CurLine); yyterminate(); }

%%
