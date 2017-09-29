%{

#if defined(EDL_PLATFORM_MSVC)
#include <io.h>	// isatty
#endif
#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"

extern void BeginToken(const char*);

#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng); BeginToken(yylval.string->c_str())
#define TOKEN(t) BeginToken(yytext); return yylval.token = t

unsigned int g_CurLine=1;		/* For error handling */

extern "C" int yywrap() { return 1; }

extern void SkipChar();
extern void PrintError(const char*,...);
extern int GetNextChar(char*,int);

#define YY_INPUT(buf,result,max_size)  {\
    result = GetNextChar(buf, max_size); \
    if (  result <= 0  ) \
      result = YY_NULL; \
    }

 
%}

%%

DECLARE					{ TOKEN(TOK_DECLARE); }
HANDLER					{ TOKEN(TOK_HANDLER); }
STATES					{ TOKEN(TOK_STATES); }
STATE					{ TOKEN(TOK_STATE); }
ALIAS					{ TOKEN(TOK_ALIAS); }
IF						{ TOKEN(TOK_IF); }
ELSE					{ TOKEN(TOK_ELSE); }
NEXT					{ TOKEN(TOK_NEXT); }
PUSH					{ TOKEN(TOK_PUSH); }
POP						{ TOKEN(TOK_POP); }
INSTRUCTION				{ TOKEN(TOK_INSTRUCTION); }
EXECUTE					{ TOKEN(TOK_EXECUTE); }
ROL						{ TOKEN(TOK_ROL); }
ROR						{ TOKEN(TOK_ROR); }
MAPPING					{ TOKEN(TOK_MAPPING); }
AFFECT					{ TOKEN(TOK_AFFECT); }
AS						{ TOKEN(TOK_AS); }
PIN						{ TOKEN(TOK_PIN); }
FUNCTION				{ TOKEN(TOK_FUNCTION); }
CALL					{ TOKEN(TOK_CALL); }

INSTANCE				{ TOKEN(TOK_INSTANCE); }

IN						{ TOKEN(TOK_IN); }
OUT						{ TOKEN(TOK_OUT); }
BIDIRECTIONAL			{ TOKEN(TOK_BIDIRECTIONAL); }

ALWAYS					{ TOKEN(TOK_ALWAYS); }
CHANGED					{ TOKEN(TOK_CHANGED); }
TRANSITION				{ TOKEN(TOK_TRANSITION); }


CONNECT					{ TOKEN(TOK_CONNECT); }
CLOCK_GEN				{ TOKEN(TOK_CLOCK_GEN); }
PULLUP					{ TOKEN(TOK_PULLUP); }
BUS_TAP					{ TOKEN(TOK_BUS_TAP); }
HIGH_IMPEDANCE			{ TOKEN(TOK_HIGH_IMPEDANCE); }
ISOLATION				{ TOKEN(TOK_ISOLATION); }

INTERNAL				{ TOKEN(TOK_INTERNAL); }

ZERO					{ TOKEN(TOK_ZERO); }
NONZERO					{ TOKEN(TOK_NONZERO); }
SIGN					{ TOKEN(TOK_SIGN); }
NOSIGN					{ TOKEN(TOK_NOSIGN); }
PARITYEVEN				{ TOKEN(TOK_PARITYEVEN); }
PARITYODD				{ TOKEN(TOK_PARITYODD); }
CARRY					{ TOKEN(TOK_CARRY); }
NOCARRY					{ TOKEN(TOK_NOCARRY); }
BIT						{ TOKEN(TOK_BIT); }
INVBIT					{ TOKEN(TOK_INVBIT); }
FORCERESET				{ TOKEN(TOK_FORCERESET); }
FORCESET				{ TOKEN(TOK_FORCESET); }
OVERFLOW				{ TOKEN(TOK_OVERFLOW); }
NOOVERFLOW				{ TOKEN(TOK_NOOVERFLOW); }

DEBUG_TRACE				{ TOKEN(TOK_TRACE); }
BASE					{ TOKEN(TOK_BASE); }

C_FUNC_EXTERN			{ TOKEN(TOK_C_FUNC_EXTERN); }

[a-zA-Z_][a-zA-Z0-9_]*	{ SAVE_TOKEN; return TOK_IDENTIFIER; }
[0-9]+					{ SAVE_TOKEN; return TOK_INTEGER; /* decimal */}
\%[01]+					{ SAVE_TOKEN; return TOK_INTEGER; /* binary */}
\$[0-9a-fA-F]+			{ SAVE_TOKEN; return TOK_INTEGER; /* hexadecimal */}
\"(\\.|[^"])*\"			{ SAVE_TOKEN; return TOK_STRING; }

"<->"					{ TOKEN(TOK_EXCHANGE); }
"[["					{ TOKEN(TOK_INDEXOPEN); }
"]]"					{ TOKEN(TOK_INDEXCLOSE); }
"++"					{ TOKEN(TOK_DADD); }
"--"					{ TOKEN(TOK_DSUB); }
".."					{ TOKEN(TOK_DDOT); }
"!="					{ TOKEN(TOK_CMPNEQ); }
"=="					{ TOKEN(TOK_CMPEQ); }
"//"					{ TOKEN(TOK_SDIV); }
"%%"					{ TOKEN(TOK_SMOD); }
"{"						{ TOKEN(TOK_LBRACE); }
"}"						{ TOKEN(TOK_RBRACE); }
"["						{ TOKEN(TOK_LSQR); }
"]"						{ TOKEN(TOK_RSQR); }
":"						{ TOKEN(TOK_COLON); }
";"						{ TOKEN(TOK_EOS); }
","						{ TOKEN(TOK_COMMA); }
"<-"					{ TOKEN(TOK_ASSIGNLEFT); }
"->"					{ TOKEN(TOK_ASSIGNRIGHT); }
"+"						{ TOKEN(TOK_ADD); }
"-"						{ TOKEN(TOK_SUB); }
"*"						{ TOKEN(TOK_MUL); }
"/"						{ TOKEN(TOK_DIV); }
"%"						{ TOKEN(TOK_MOD); }
"("						{ TOKEN(TOK_OBR); }
")"						{ TOKEN(TOK_CBR); }
"|"						{ TOKEN(TOK_BAR); }
"&"						{ TOKEN(TOK_AMP); }
"^"						{ TOKEN(TOK_HAT); }
"."						{ TOKEN(TOK_DOT); }
"@"						{ TOKEN(TOK_AT); }
"~"						{ TOKEN(TOK_TILDE); }
"<="					{ TOKEN(TOK_CMPLESSEQ); }
"<"						{ TOKEN(TOK_CMPLESS); }
">="					{ TOKEN(TOK_CMPGREATEREQ); }
">"						{ TOKEN(TOK_CMPGREATER); }

[ \t\r\n]				{ SkipChar(); /* Consume white space */ }
#.*						{ /* Ignore comments */ }

.						{ PrintError("Unrecognised character in input"); SkipChar(); }

%%
