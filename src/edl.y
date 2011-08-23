%{
	#include "ast.h"
        #include <cstdio>
        #include <cstdlib>

	CBlock *g_ProgramBlock; /* the top level root node of our final AST */

	extern unsigned int g_CurLine;

	#define YYERROR_VERBOSE	/* Better error reporting */

	extern int yylex();
	void yyerror(const char *s) { std::printf("Error: %s (line %d)\n",s,g_CurLine); }
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
	CStateIdent *state_ident;
	COperand *operand;
	CMapping *mapping;
	COperandPartial *opPartial;
	CAffect *affect;
	CTrigger *trigger;
	CExternDecl *extern_c_decl;
	std::vector<COperand*> *opervec;
	std::vector<CAliasDeclaration*> *aliasvec;
	std::vector<CStateDeclaration*> *varvec;
	std::vector<CExpression*> *exprvec;
	std::vector<CDebugTrace*> *debugvec;
	std::vector<CStateIdent*> *staterefvec;
	std::vector<CMapping*> *mappingvec;
	std::vector<CAffect*> *affectvec;
	std::vector<CInteger*> *externparams;
	std::vector<CExpression*> *params;
	std::vector<CParamDecl*> *namedParams;
	std::string *string;
	int token;
}

%token <string> TOK_IDENTIFIER TOK_INTEGER TOK_STRING
%token <token>	TOK_DECLARE TOK_HANDLER	TOK_STATES TOK_STATE TOK_ALIAS TOK_IF TOK_NEXT TOK_PUSH TOK_POP	/* Reserved words */
%token <token>	TOK_INSTRUCTION	TOK_EXECUTE TOK_ROL TOK_ROR TOK_MAPPING TOK_AFFECT TOK_AS		/* Reserved words */
%token <token>	TOK_PIN TOK_IN TOK_OUT TOK_BIDIRECTIONAL TOK_INSTANCE					/* Reserved words */
%token <token>	TOK_ALWAYS TOK_CHANGED TOK_TRANSITION TOK_INTERNAL TOK_FUNCTION TOK_CALL		/* Reserved words */
%token <token>	TOK_ZERO TOK_SIGN TOK_PARITYEVEN TOK_PARITYODD TOK_CARRY TOK_BIT			/* Reserved words AFFECTORS */
%token <token>	TOK_TRACE TOK_BASE									/* Debug reserved words */
%token <token>	TOK_C_FUNC_EXTERN									/* C Calling Interface */
%token <token> TOK_LSQR TOK_RSQR TOK_LBRACE TOK_RBRACE TOK_COMMA TOK_COLON TOK_EOS 			/* Operators/Seperators */
%token <token> TOK_ASSIGNLEFT TOK_ASSIGNRIGHT TOK_ADD TOK_SUB TOK_OBR TOK_CBR TOK_CMPEQ TOK_BAR		/* Operators/Seperators */
%token <token> TOK_DOT TOK_AT TOK_AMP TOK_TILDE TOK_DDOT TOK_CMPNEQ TOK_HAT				/* Operators/Seperators */
%token <token> TOK_CMPLESSEQ TOK_CMPLESS TOK_CMPGREATEREQ TOK_CMPGREATER				/* Operators/Seperators */

%type <namedParams> named_params_list named_return
%type <trigger> trigger
%type <token> pin_type
%type <params> params
%type <externparams> params_list return_val
%type <affect> affector
%type <mapping> mapping
%type <operand> operand 
%type <opPartial> partialOperands
%type <strng> quoted
%type <ident> ident
%type <ident> ident_ref
%type <state_ident> state_ident
%type <intgr> numeric 
%type <expr> expr
%type <varvec> states_list
%type <aliasvec> aliases
%type <debugvec> debuglist
%type <opervec> operandList
%type <mappingvec> mappingList
%type <affectvec> affectors
%type <staterefvec> state_ident_list
/* %type <exprvec> call_args */
%type <block> program block stmts 
%type <stmt> stmt var_decl handler_decl state_decl states_decl state_def alias_decl ifblock debug instruction_decl mapping_decl extern_c_decl function_decl

%right TOK_ASSIGNRIGHT
%left TOK_ASSIGNLEFT
%right TOK_TILDE
%left TOK_LSQR
%left TOK_BAR TOK_AMP TOK_HAT
%left TOK_CMPEQ TOK_CMPNEQ TOK_CMPLESS TOK_CMPLESSEQ TOK_CMPGREATER TOK_CMPGREATEREQ
%left TOK_ADD TOK_SUB

%start program

%%

program : stmts { g_ProgramBlock = $1; }
		;
		
stmts : stmt { $$ = new CBlock(); $$->statements.push_back($<stmt>1); }
	  | stmts stmt { $1->statements.push_back($<stmt>2); }
	  ;

stmt : TOK_INSTANCE quoted TOK_AS ident TOK_EOS { $$ = new CInstance(*$2,*$4); }
     | var_decl TOK_EOS
     | states_decl
     | state_def
     | handler_decl
     | function_decl
     | extern_c_decl
     | instruction_decl
     | mapping_decl
     | ifblock
     | TOK_EXECUTE ident TOK_EOS { $$ = new CExecute(*$2); }
     | TOK_NEXT state_ident_list TOK_EOS { $$ = new CStateJump(*$2); delete $2; }
     | TOK_PUSH state_ident_list TOK_EOS { $$ = new CStatePush(*$2); delete $2; }
     | TOK_POP ident TOK_EOS { $$ = new CStatePop(*$2); }
     | TOK_TRACE debuglist TOK_EOS { $$ = new CDebugLine(*$2); delete $2; }
     | expr TOK_EOS { $$ = new CExpressionStatement(*$1); }
     ;

params_list : params_list TOK_COMMA TOK_LSQR numeric TOK_RSQR { $$->push_back($<intgr>4); }
	    | TOK_LSQR numeric TOK_RSQR { $$ = new ExternParamsList(); $$->push_back($<intgr>2); }
            | { $$ = new ExternParamsList(); }
	;

return_val : TOK_LSQR numeric TOK_RSQR { $$ = new ExternParamsList(); $$->push_back($<intgr>2); }
	   ;

extern_c_decl : TOK_C_FUNC_EXTERN return_val ident params_list TOK_EOS	{ $$ = new CExternDecl(*$2,*$3,*$4); delete $4; }
	      | TOK_C_FUNC_EXTERN ident params_list TOK_EOS             { $$ = new CExternDecl(*$2,*$3); delete $3; }
	      ;

named_params_list : named_params_list TOK_COMMA ident TOK_LSQR numeric TOK_RSQR { $$->push_back(new CParamDecl(*$3,*$5)); }
		  | ident TOK_LSQR numeric TOK_RSQR { $$ = new NamedParamsList(); $$->push_back(new CParamDecl(*$1,*$3)); }
		  | { $$ = new NamedParamsList(); }
		  ;

named_return : ident TOK_LSQR numeric TOK_RSQR { $$ = new NamedParamsList(); $$->push_back(new CParamDecl(*$1,*$3)); }
	     ;

function_decl : TOK_FUNCTION TOK_INTERNAL named_return ident named_params_list block { $$ = new CFunctionDecl(true,*$3,*$4,*$5,*$6); delete $5; }
	      | TOK_FUNCTION named_return ident named_params_list block		{ $$ = new CFunctionDecl(false,*$2,*$3,*$4,*$5); delete $4; }
              | TOK_FUNCTION TOK_INTERNAL ident named_params_list block { $$ = new CFunctionDecl(true,*$3,*$4,*$5); delete $4; }
	      | TOK_FUNCTION ident named_params_list block		{ $$ = new CFunctionDecl(false,*$2,*$3,*$4); delete $3; }
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

state_ident : TOK_IDENTIFIER { $$ = new CStateIdent(*$1); delete $1; }
	    ;

state_ident_list : state_ident_list TOK_DOT state_ident { $$->push_back($<state_ident>3); }
		 | state_ident { $$ = new StateIdentList(); $$->push_back($<state_ident>1); }
		;

block : TOK_LBRACE stmts TOK_RBRACE {$$ = $2; }
      | TOK_LBRACE TOK_RBRACE { $$ = new CBlock(); }

trigger: TOK_ALWAYS 							{ $$ = new CTrigger(TOK_ALWAYS); }
       | TOK_CHANGED							{ $$ = new CTrigger(TOK_CHANGED); }
       | TOK_TRANSITION TOK_OBR numeric TOK_COMMA numeric TOK_CBR	{ $$ = new CTrigger(TOK_TRANSITION,*$3,*$5); }
       ;

handler_decl : TOK_HANDLER ident trigger block	{ $$ = new CHandlerDeclaration(*$2,*$3,*$4); }

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

operand : numeric { $$ = new COperandNumber(*$1); }
	| ident TOK_LSQR numeric TOK_RSQR { $$ = new COperandIdent(*$1,*$3); }
	| ident { $$ = new COperandMapping(*$1); }
	;

partialOperands : partialOperands TOK_COLON operand { $$->Add($<operand>3); }
		| operand { $$ = new COperandPartial(); $$->Add($<operand>1); }

operandList : operandList TOK_COMMA partialOperands { $$->push_back($<operand>3); }
	    | partialOperands { $$ = new OperandList(); $$->push_back($<operand>1); }
	;

instruction_decl : TOK_INSTRUCTION quoted operandList block { $$ = new CInstruction(*$2,*$3,*$4); }
		 ;

mapping : numeric quoted expr TOK_EOS { $$ = new CMapping(*$1,*$2,*$3); }
	;

mappingList : mappingList mapping { $$->push_back($<mapping>2); }
	 | mapping { $$ = new MappingList(); $$->push_back($<mapping>1); }
	;

mapping_decl : TOK_MAPPING ident TOK_LSQR numeric TOK_RSQR TOK_LBRACE mappingList TOK_RBRACE { $$ = new CMappingDeclaration(*$2,*$4,*$7); } 
	     ;

pin_type: TOK_IN
	| TOK_OUT
	| TOK_BIDIRECTIONAL
	;

var_decl : TOK_DECLARE TOK_INTERNAL ident TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(true, *$3, *$5); }
	| TOK_DECLARE TOK_INTERNAL ident TOK_LSQR numeric TOK_RSQR TOK_ALIAS aliases { $$ = new CVariableDeclaration(true, *$3, *$5, *$8); delete $8; }
	| TOK_DECLARE ident TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(false, *$2, *$4); }
	| TOK_DECLARE ident TOK_LSQR numeric TOK_RSQR TOK_ALIAS aliases { $$ = new CVariableDeclaration(false, *$2, *$4, *$7); delete $7; }
	| TOK_PIN pin_type ident TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration($2,*$3,*$5); }
	| TOK_PIN pin_type ident TOK_LSQR numeric TOK_RSQR TOK_ALIAS aliases { $$ = new CVariableDeclaration($2,*$3,*$5,*$8); delete $8; }
	;

affector : ident TOK_AS TOK_ZERO { $$ = new CAffect(*$1,TOK_ZERO); }
	 | ident TOK_AS TOK_SIGN { $$ = new CAffect(*$1,TOK_SIGN); }
	 | ident TOK_AS TOK_PARITYEVEN { $$ = new CAffect(*$1,TOK_PARITYEVEN); }
	 | ident TOK_AS TOK_PARITYODD { $$ = new CAffect(*$1,TOK_PARITYODD); }
	 | ident TOK_AS TOK_BIT TOK_OBR numeric TOK_CBR { $$ = new CAffect(*$1,TOK_BIT,*$5); }
	 | ident TOK_AS TOK_CARRY TOK_OBR numeric TOK_CBR { $$ = new CAffect(*$1,TOK_CARRY,*$5); }
	 ;

affectors : affectors TOK_COMMA affector { $$->push_back($<affect>3); }
	  | affector { $$ = new AffectorList(); $$->push_back($<affect>1); }
	;

params : params TOK_COMMA expr	{ $$->push_back($3); }
       | expr			{ $$ = new ParamsList(); $$->push_back($1); }
	|			{ $$ = new ParamsList(); }
	;

expr : ident_ref TOK_ASSIGNLEFT expr { $$ = new CAssignment(*$<ident>1,*$3); }
     | expr TOK_ASSIGNRIGHT ident_ref { $$ = new CAssignment(*$<ident>3,*$1); }
     | expr TOK_ADD expr { $$ = new CBinaryOperator(*$1,TOK_ADD,*$3); }
     | expr TOK_SUB expr { $$ = new CBinaryOperator(*$1,TOK_SUB,*$3); }
     | expr TOK_CMPEQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPEQ,*$3); }
     | expr TOK_CMPNEQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPNEQ,*$3); }
     | expr TOK_CMPLESSEQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPLESSEQ,*$3); }
     | expr TOK_CMPLESS expr { $$ = new CBinaryOperator(*$1,TOK_CMPLESS,*$3); }
     | expr TOK_CMPGREATEREQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPGREATEREQ,*$3); }
     | expr TOK_CMPGREATER expr { $$ = new CBinaryOperator(*$1,TOK_CMPGREATER,*$3); }
     | expr TOK_BAR expr { $$ = new CBinaryOperator(*$1,TOK_BAR,*$3); }
     | expr TOK_AMP expr { $$ = new CBinaryOperator(*$1,TOK_AMP,*$3); }
     | expr TOK_HAT expr { $$ = new CBinaryOperator(*$1,TOK_HAT,*$3); }
     | state_ident_list TOK_AT { $$ = new CStateTest(*$1); }
     | TOK_TILDE expr { $$ = new CBinaryOperator(*$2,TOK_TILDE,*$2); }
     | TOK_ROL TOK_OBR expr TOK_COMMA ident_ref TOK_COMMA expr TOK_COMMA expr TOK_CBR { $$ = new CRotationOperator(TOK_ROL,*$3,*$5,*$7,*$9); }
     | TOK_ROR TOK_OBR expr TOK_COMMA ident_ref TOK_COMMA expr TOK_COMMA expr TOK_CBR { $$ = new CRotationOperator(TOK_ROR,*$3,*$5,*$7,*$9); }
     | expr TOK_LSQR numeric TOK_RSQR { $3->Decrement(); $$ = new CCastOperator(*$1,*$3); }
     | expr TOK_LSQR numeric TOK_DDOT numeric TOK_RSQR { $$ = new CCastOperator(*$1,*$3,*$5); }
     | TOK_CALL ident_ref TOK_OBR params TOK_CBR { $$ = new CFuncCall(*$2,*$4); }
     | TOK_AFFECT affectors TOK_LBRACE expr TOK_RBRACE { $$ = new CAffector(*$2,*$4); }
     | ident_ref { $<ident>$ = $1; }
     | numeric { $$ = new CInteger(*$1); delete $1; }
     | TOK_OBR expr TOK_CBR { $$ = $2; }
	;

quoted : TOK_STRING { $$ = new CString(*$1); delete $1; }
       ;

ident : TOK_IDENTIFIER { $$ = new CIdentifier(*$1); delete $1; }
	  ;

ident_ref : TOK_IDENTIFIER { $$ = new CIdentifier(*$1); }
	  | TOK_IDENTIFIER TOK_IDENTIFIER { $$ = new CIdentifier(*$1,*$2); }
	  ;

numeric : TOK_INTEGER { $$ = new CInteger(*$1); delete $1; }
		;

%%
