%{
	#include "ast.h"
        #include <cstdio>
        #include <cstdlib>

	CBlock *g_ProgramBlock; /* the top level root node of our final AST */

	extern unsigned int g_CurLine;

	#define YYERROR_VERBOSE	/* Better error reporting */

	extern int yylex();
	void yyerror(const char *s) { std::printf("Error: %s (line %d)\n",s,g_CurLine);std::exit(-1); }
%}

%union {
	CNode *node;
	CBlock *block;
	CExpression *expr;
	CStatement *stmt;
	CInteger   *intgr;
	CIdentifier *ident;
	CVariableDeclaration *var_decl;
	CHandlerDeclaration *handler_decl;
	CStateDeclaration *state_decl;
	std::vector<CStateDeclaration*> *varvec;
	std::vector<CExpression*> *exprvec;
	std::string *string;
	int token;
}

%token <string> TOK_IDENTIFIER TOK_INTEGER
%token <token>	TOK_DECLARE TOK_HANDLER	TOK_STATES				/* Reserved words */
%token <token> TOK_LSQR TOK_RSQR TOK_LBRACE TOK_RBRACE TOK_COMMA TOK_EOS

%type <ident> ident
%type <intgr> numeric 
%type <varvec> states_list
/* %type <exprvec> call_args */
%type <block> program block stmts
%type <stmt> stmt var_decl handler_decl state_decl

%start program

%%

program : stmts { g_ProgramBlock = $1; }
		;
		
stmts : stmt { $$ = new CBlock(); $$->statements.push_back($<stmt>1); }
	  | stmts stmt { $1->statements.push_back($<stmt>2); }
	  ;

stmt : var_decl TOK_EOS
     | handler_decl
     ;

block : TOK_LBRACE stmts TOK_RBRACE {$$ = $2; }
      | TOK_LBRACE TOK_RBRACE { $$ = new CBlock(); }

handler_decl : TOK_HANDLER ident block	{ $$ = new CHandlerDeclaration(*$2,*$3); }

state_decl : ident { $$ = new CStateDeclaration(*$1); }

states_list : states_list TOK_COMMA state_decl { $1->push_back($<state_decl>3); }
	    | state_decl { $$ = new StateList(); $$->push_back($<state_decl>1); }
		;

var_decl : TOK_DECLARE ident TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(*$2, *$4); }
	 | TOK_STATES states_list { $$ = new CStatesDeclaration(*$2); delete $2; }
		 ;
		
ident : TOK_IDENTIFIER { $$ = new CIdentifier(*$1); delete $1; }
	  ;

numeric : TOK_INTEGER { $$ = new CInteger(*$1); delete $1; }
		;

%%
