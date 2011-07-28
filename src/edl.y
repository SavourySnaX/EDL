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
	CString	   *strng;
	CVariableDeclaration *var_decl;
	CHandlerDeclaration *handler_decl;
	CStateDeclaration *state_decl;
	CAliasDeclaration *alias_decl;
	CDebugTrace *debug;
	std::vector<CAliasDeclaration*> *aliasvec;
	std::vector<CStateDeclaration*> *varvec;
	std::vector<CExpression*> *exprvec;
	std::vector<CDebugTrace*> *debugvec;
	std::string *string;
	int token;
}

%token <string> TOK_IDENTIFIER TOK_INTEGER TOK_STRING
%token <token>	TOK_DECLARE TOK_HANDLER	TOK_STATES TOK_STATE TOK_ALIAS TOK_IF			/* Reserved words */
%token <token>	TOK_TRACE TOK_BASE								/* Debug reserved words */
%token <token> TOK_LSQR TOK_RSQR TOK_LBRACE TOK_RBRACE TOK_COMMA TOK_COLON TOK_EOS TOK_ASSIGNLEFT TOK_ASSIGNRIGHT TOK_ADD TOK_SUB TOK_OBR TOK_CBR TOK_CMPEQ TOK_BAR

%type <strng> quoted
%type <ident> ident
%type <intgr> numeric 
%type <expr> expr
%type <varvec> states_list
%type <aliasvec> aliases
%type <debugvec> debuglist
/* %type <exprvec> call_args */
%type <block> program block stmts
%type <stmt> stmt var_decl handler_decl state_decl states_decl state_def alias_decl ifblock debug

%right TOK_ASSIGNRIGHT
%left TOK_ASSIGNLEFT
%left TOK_CMPEQ
%left TOK_ADD TOK_SUB

%start program

%%

program : stmts { g_ProgramBlock = $1; }
		;
		
stmts : stmt { $$ = new CBlock(); $$->statements.push_back($<stmt>1); }
	  | stmts stmt { $1->statements.push_back($<stmt>2); }
	  ;

stmt : var_decl TOK_EOS
     | states_decl
     | state_def
     | handler_decl
     | ifblock
     | TOK_TRACE debuglist TOK_EOS { $$ = new CDebugLine(*$2); delete $2; }
     | expr TOK_EOS { $$ = new CExpressionStatement(*$1); }
     ;

debug : quoted { $$ = new CDebugTraceString(*$1); }
      | numeric { $$ = new CDebugTraceInteger(*$1); }
      | ident { $$ = new CDebugTraceIdentifier(*$1); }
      | TOK_BASE numeric { $$ = new CDebugTraceBase(*$2); }
      ;

debuglist : debuglist TOK_COMMA debug { $1->push_back($<debug>3); }
	  | debug { $$ = new DebugList(); $$->push_back($<debug>1); }
	;

ifblock : TOK_IF expr block { $$ = new CIfStatement(*$2,*$3); }
	;

block : TOK_LBRACE stmts TOK_RBRACE {$$ = $2; }
      | TOK_LBRACE TOK_RBRACE { $$ = new CBlock(); }

handler_decl : TOK_HANDLER ident block	{ $$ = new CHandlerDeclaration(*$2,*$3); }

state_decl : ident { $$ = new CStateDeclaration(*$1); }

states_list : states_list TOK_COMMA state_decl { $1->back()->autoIncrement=true; $1->push_back($<state_decl>3); }
	    | states_list TOK_BAR state_decl { $1->push_back($<state_decl>3); }
	    | state_decl { $$ = new StateList(); $$->push_back($<state_decl>1); }
		;

states_decl : TOK_STATES states_list block { $$ = new CStatesDeclaration(*$2,*$3); delete $2; }
		;

state_def : TOK_STATE ident block { $$ = new CStateDefinition(*$2,*$3); }
	  ;

alias_decl : ident TOK_LSQR numeric TOK_RSQR { $$ = new CAliasDeclaration(*$1,*$3); }
	  | numeric { $$ = new CAliasDeclaration(*$1); }
	;

aliases : aliases TOK_COLON alias_decl { $$->push_back($<alias_decl>3); }
	| alias_decl { $$ = new AliasList(); $$->push_back($<alias_decl>1); }
	;

var_decl : TOK_DECLARE ident TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(*$2, *$4); }
	| TOK_DECLARE ident TOK_LSQR numeric TOK_RSQR TOK_ALIAS aliases { $$ = new CVariableDeclaration(*$2, *$4, *$7); delete $7; }
	;

expr : ident TOK_ASSIGNLEFT expr { $$ = new CAssignment(*$<ident>1,*$3); }
     | expr TOK_ASSIGNRIGHT ident { $$ = new CAssignment(*$<ident>3,*$1); }
     | expr TOK_ADD expr { $$ = new CBinaryOperator(*$1,TOK_ADD,*$3); }
     | expr TOK_SUB expr { $$ = new CBinaryOperator(*$1,TOK_SUB,*$3); }
     | expr TOK_CMPEQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPEQ,*$3); }
     | ident { $<ident>$ = $1; }
     | numeric { $$ = new CInteger(*$1); delete $1; }
     | TOK_OBR expr TOK_CBR { $$ = $2; }
	;

quoted : TOK_STRING { $$ = new CString(*$1); delete $1; }
       ;

ident : TOK_IDENTIFIER { $$ = new CIdentifier(*$1); delete $1; }
	  ;

numeric : TOK_INTEGER { $$ = new CInteger(*$1); delete $1; }
		;

%%
