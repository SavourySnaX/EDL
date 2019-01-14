%{
#include "yyltype.h"
#include "ast.h"
#include <cstdio>
#include <cstdlib>

	CBlock *g_ProgramBlock; /* the top level root node of our final AST */

	extern unsigned int g_CurLine;

	#define YYERROR_VERBOSE	/* Better error reporting */

	extern int yylex();
	extern void PrintError(const char *errorstring, ...);

	void yyerror(const char *s) { PrintError(s); }
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
	CConnect *connect;
	std::vector<COperandPartial*> *opervec;
	std::vector<CAliasDeclaration*> *aliasvec;
	std::vector<std::vector<CAliasDeclaration*> > *vecaliasvec;
	std::vector<CStateDeclaration*> *varvec;
	std::vector<CExpression*> *exprvec;
	std::vector<CDebugTrace*> *debugvec;
	std::vector<CStateIdent*> *staterefvec;
	std::vector<CMapping*> *mappingvec;
	std::vector<CAffect*> *affectvec;
	std::vector<CInteger*> *externparams;
	std::vector<CExpression*> *params;
	std::vector<CConnect*> *connectvec;
	std::vector<CParamDecl*> *namedParams;
	std::string *string;
	int token;
}

%token <string> TOK_IDENTIFIER TOK_INTEGER TOK_STRING
%token <token>	TOK_DECLARE TOK_HANDLER	TOK_STATES TOK_STATE TOK_ALIAS TOK_IF TOK_ELSE TOK_NEXT TOK_PUSH TOK_POP	/* Reserved words */
%token <token>	TOK_INSTRUCTION	TOK_EXECUTE TOK_ROL TOK_ROR TOK_MAPPING TOK_AFFECT TOK_AS		/* Reserved words */
%token <token>	TOK_PIN TOK_IN TOK_OUT TOK_BIDIRECTIONAL TOK_INSTANCE TOK_EXCHANGE TOK_DADD TOK_DSUB	/* Reserved words */
%token <token>	TOK_ALWAYS TOK_CHANGED TOK_TRANSITION TOK_INTERNAL TOK_FUNCTION TOK_CALL		/* Reserved words */
%token <token>	TOK_ZERO TOK_SIGN TOK_PARITYEVEN TOK_PARITYODD TOK_CARRY TOK_BIT TOK_OVERFLOW			/* Reserved words AFFECTORS */
%token <token>	TOK_NONZERO TOK_NOSIGN TOK_NOCARRY TOK_INVBIT TOK_FORCERESET TOK_FORCESET TOK_NOOVERFLOW	/* Reserved words AFFECTORS */
%token <token>	TOK_TRACE TOK_BASE									/* Debug reserved words */
%token <token>	TOK_C_FUNC_EXTERN									/* C Calling Interface */
%token <token> TOK_LSQR TOK_RSQR TOK_LBRACE TOK_RBRACE TOK_COMMA TOK_COLON TOK_EOS 			/* Operators/Seperators */
%token <token> TOK_ASSIGNLEFT TOK_ASSIGNRIGHT TOK_ADD TOK_SUB TOK_OBR TOK_CBR TOK_CMPEQ TOK_BAR		/* Operators/Seperators */
%token <token> TOK_DOT TOK_AT TOK_AMP TOK_TILDE TOK_DDOT TOK_CMPNEQ TOK_HAT TOK_MUL TOK_DIV TOK_MOD	/* Operators/Seperators */
%token <token> TOK_SDIV TOK_SMOD									/* Operators/Seperators */
%token <token> TOK_CMPLESSEQ TOK_CMPLESS TOK_CMPGREATEREQ TOK_CMPGREATER TOK_INDEXOPEN TOK_INDEXCLOSE	/* Operators/Seperators */
%token <token>	TOK_CONNECT TOK_CLOCK_GEN TOK_PULLUP TOK_BUS_TAP TOK_HIGH_IMPEDANCE TOK_ISOLATION		/* Reserved words schematic */

%type <namedParams> named_params_list named_return
%type <trigger> trigger
%type <token> pin_type
%type <params> params connect_params
%type <connect> connect
%type <externparams> params_list return_val var_inits
%type <affect> affector
%type <mapping> mapping
%type <operand> operand 
%type <opPartial> partialOperands
%type <strng> quoted
%type <ident> ident
%type <ident> ident_ref
%type <state_ident> state_ident
%type <intgr> numeric 
%type <expr> expr connect_expr
%type <varvec> states_list
%type <aliasvec> aliases
%type <vecaliasvec> alias_list
%type <debugvec> debuglist
%type <opervec> operandList
%type <mappingvec> mappingList
%type <affectvec> affectors
%type <staterefvec> state_ident_list
%type <connectvec> connect_list

/* %type <exprvec> call_args */
%type <block> program block stmts 
%type <stmt> stmt var_decl handler_decl state_decl states_decl state_def alias_decl ifblock debug instruction_decl mapping_decl extern_c_decl function_decl connect_decl

%right TOK_ASSIGNRIGHT
%left TOK_ASSIGNLEFT
%right TOK_TILDE
%left TOK_LSQR
%left TOK_BAR TOK_AMP TOK_HAT
%left TOK_CMPEQ TOK_CMPNEQ TOK_CMPLESS TOK_CMPLESSEQ TOK_CMPGREATER TOK_CMPGREATEREQ
%left TOK_MUL TOK_DIV TOK_SDIV TOK_MOD TOK_SMOD
%left TOK_ADD TOK_DADD TOK_SUB TOK_DSUB

%start program

%%

program : stmts { g_ProgramBlock = $1; }
		;
		
stmts : stmt { $$ = new CBlock(); $$->statements.push_back($<stmt>1); }
	  | stmts stmt { $1->statements.push_back($<stmt>2); }
	  ;

var_inits : numeric { $$ = new ExternParamsList(); $$->push_back($<intgr>1); }
	  | var_inits TOK_COMMA numeric { $$->push_back($<intgr>3); }

stmt : TOK_INSTANCE quoted TOK_AS ident TOK_EOS { $$ = new CInstance(*$2,*$4); }
     | var_decl TOK_EOS
     | var_decl TOK_LBRACE var_inits TOK_RBRACE TOK_EOS { $<var_decl>1->AddInitialisers(*$3); delete $3; }
     | states_decl
     | state_def
     | handler_decl
     | connect_decl
     | function_decl
     | extern_c_decl
     | instruction_decl
     | mapping_decl
     | ifblock
     | ident TOK_EXCHANGE ident TOK_EOS { $$ = new CExchange(*$1,*$3,&@2); }
     | TOK_EXECUTE ident ident TOK_EOS { $$ = new CExecute(*$2,*$3,&@1); }
     | TOK_EXECUTE ident TOK_EOS { $$ = new CExecute(*$2,&@1); }
     | TOK_NEXT state_ident_list TOK_EOS { $$ = new CStateJump(*$2); delete $2; }
     | TOK_PUSH state_ident_list TOK_EOS { $$ = new CStatePush(*$2); delete $2; }
     | TOK_POP ident TOK_EOS { $$ = new CStatePop(*$2); }
     | TOK_TRACE debuglist TOK_EOS { $$ = new CDebugLine(*$2,&@1); delete $2; }
     | expr TOK_EOS { $$ = new CExpressionStatement(*$1); }
     ;

params_list : params_list TOK_COMMA TOK_LSQR numeric TOK_RSQR { $$->push_back($<intgr>4); }
	    | TOK_LSQR numeric TOK_RSQR { $$ = new ExternParamsList(); $$->push_back($<intgr>2); }
            | { $$ = new ExternParamsList(); }
	;

return_val : TOK_LSQR numeric TOK_RSQR { $$ = new ExternParamsList(); $$->push_back($<intgr>2); }
	   ;

extern_c_decl : TOK_C_FUNC_EXTERN return_val ident params_list TOK_EOS	{ $$ = new CExternDecl(*$2,*$3,*$4,CombineTokenLocations(@1,@5)); delete $4; }
	      | TOK_C_FUNC_EXTERN ident params_list TOK_EOS             { $$ = new CExternDecl(*$2,*$3,CombineTokenLocations(@1,@4)); delete $3; }
	      ;

named_params_list : named_params_list TOK_COMMA ident TOK_LSQR numeric TOK_RSQR { $$->push_back(new CParamDecl(*$3,*$5)); }
		  | ident TOK_LSQR numeric TOK_RSQR { $$ = new NamedParamsList(); $$->push_back(new CParamDecl(*$1,*$3)); }
		  | { $$ = new NamedParamsList(); }
		  ;

named_return : ident TOK_LSQR numeric TOK_RSQR { $$ = new NamedParamsList(); $$->push_back(new CParamDecl(*$1,*$3)); }
	     ;

function_decl : TOK_FUNCTION TOK_INTERNAL named_return ident named_params_list block { $$ = new CFunctionDecl(true,*$3,*$4,*$5,*$6,&@1); delete $5; }
	      | TOK_FUNCTION named_return ident named_params_list block		{ $$ = new CFunctionDecl(false,*$2,*$3,*$4,*$5,&@1); delete $4; }
              | TOK_FUNCTION TOK_INTERNAL ident named_params_list block { $$ = new CFunctionDecl(true,*$3,*$4,*$5,&@1); delete $4; }
	      | TOK_FUNCTION ident named_params_list block		{ $$ = new CFunctionDecl(false,*$2,*$3,*$4,&@1); delete $3; }
	      ;

debug : quoted { $$ = new CDebugTraceString(*$1); }
      | numeric { $$ = new CDebugTraceInteger(*$1); }
      | ident_ref { $$ = new CDebugTraceIdentifier(*$1); }
      | TOK_BASE numeric { $$ = new CDebugTraceBase(*$2); }
      ;

debuglist : debuglist TOK_COMMA debug { $1->push_back($<debug>3); }
	  | debug { $$ = new DebugList(); $$->push_back($<debug>1); }
	;

ifblock : TOK_IF expr block { $$ = new CIfStatement(*$2,*$3); }
        | TOK_IF expr block TOK_ELSE block { $$ = new CIfStatement(*$2,*$3,*$5); }
	;

state_ident : TOK_IDENTIFIER { $$ = new CStateIdent(*$1,&@1); delete $1; }
	    ;

state_ident_list : state_ident_list TOK_DOT state_ident { $$->push_back($<state_ident>3); }
		 | state_ident { $$ = new StateIdentList(); $$->push_back($<state_ident>1); }
		;

block : TOK_LBRACE stmts TOK_RBRACE {$$ = $2; $$->SetBlockLocation(&@1,&@3); }
      | TOK_LBRACE TOK_RBRACE { $$ = new CBlock(); $$->SetBlockLocation(&@1,&@2); }

trigger: TOK_ALWAYS 							{ $$ = new CTrigger(TOK_ALWAYS,&@1); }
       | TOK_CHANGED							{ $$ = new CTrigger(TOK_CHANGED,&@1); }
       | TOK_TRANSITION TOK_OBR numeric TOK_COMMA numeric TOK_CBR	{ $$ = new CTrigger(TOK_TRANSITION,&@1,*$3,*$5); }
       ;

/*	| connect_list TOK_COMMA TOK_CLOCK_GEN TOK_OBR numeric TOK_CBR	{ $$->push_back(new CClock(numeric)); }*/

connect_expr : expr								{ $$ = $1; }
    | TOK_ISOLATION								{ $$ = NULL; }
	;

connect_params : connect_params TOK_COMMA connect_expr		{ $$->push_back($3); }
	| connect_expr											{ $$ = new ParamsList(); $$->push_back($1); }
	;

connect : connect_params TOK_EOS				{ $$=new CConnect(*$1,ConnectionType::None,&@2); }
	| TOK_BUS_TAP TOK_OBR quoted TOK_COMMA numeric TOK_CBR connect_params TOK_EOS		{ $$=new CConnect(*$7,*$3,*$5,ConnectionType::None,&@1); }
	| TOK_PULLUP connect_params TOK_EOS			{ $$=new CConnect(*$2, ConnectionType::Pullup,&@1); }
	| TOK_BUS_TAP TOK_OBR quoted TOK_COMMA numeric TOK_CBR TOK_PULLUP connect_params TOK_EOS	{ $$=new CConnect(*$8,*$3,*$5,ConnectionType::Pullup,&@1); }
	;

connect_list : connect_list connect				{ $$->push_back($2); }
	| connect			{ $$ = new ConnectList(); $$->push_back($1); }
	;

connect_decl : TOK_CONNECT ident TOK_LBRACE connect_list TOK_RBRACE	{ $$ = new CConnectDeclaration(*$2,*$4,&@1,&@3,&@5); }

handler_decl : TOK_HANDLER ident trigger block	{ $$ = new CHandlerDeclaration(*$2,*$3,*$4,&@1); }

state_decl : ident { $$ = new CStateDeclaration(*$1); }

states_list : states_list TOK_COMMA state_decl { $1->back()->autoIncrement=true; $1->push_back($<state_decl>3); }
	    | states_list TOK_BAR state_decl { $1->push_back($<state_decl>3); }
	    | state_decl { $$ = new StateList(); $$->push_back($<state_decl>1); }
		;

states_decl : TOK_STATES states_list block { $$ = new CStatesDeclaration(*$2,*$3,&@1); delete $2; }
		;

state_def : TOK_STATE ident block { $$ = new CStateDefinition(*$2,*$3); }
	  ;

alias_decl : ident TOK_LSQR numeric TOK_RSQR { $$ = new CAliasDeclaration(*$1,*$3); }
	  | numeric { $$ = new CAliasDeclaration(*$1,false); }
      | TOK_LSQR numeric TOK_RSQR { $$ = new CAliasDeclaration(*$2,true); }
	;

aliases : aliases TOK_COLON alias_decl { $$->push_back($<alias_decl>3); }
	| alias_decl { $$ = new AliasList(); $$->push_back($<alias_decl>1); }
	;

alias_list : alias_list TOK_ALIAS aliases { $$->push_back(*$3); }
	| TOK_ALIAS aliases { $$ = new MultiAliasList(); $$->push_back(*$2); }
	;

operand : numeric { $$ = new COperandNumber(*$1); }
	| ident TOK_LSQR numeric TOK_RSQR { $$ = new COperandIdent(*$1,*$3); }
	| ident { $$ = new COperandMapping(*$1); }
	;

partialOperands : partialOperands TOK_COLON operand { $$->Add($<operand>3); }
		| operand { $$ = new COperandPartial(); $$->Add($<operand>1); }

operandList : operandList TOK_COMMA partialOperands { $$->push_back($3); }
	    | partialOperands { $$ = new OperandPartialList(); $$->push_back($1); }
	;

instruction_decl : TOK_INSTRUCTION ident quoted operandList block { $$ = new CInstruction(*$2,*$3,*$4,*$5, &@1); }
		 | TOK_INSTRUCTION quoted operandList block { $$ = new CInstruction(*$2,*$3,*$4, &@1); }
		 ;

mapping : numeric quoted expr TOK_EOS { $$ = new CMapping(*$1,*$2,*$3); }
	;

mappingList : mappingList mapping { $$->push_back($<mapping>2); }
	 | mapping { $$ = new MappingList(); $$->push_back($<mapping>1); }
	;

mapping_decl : TOK_MAPPING ident TOK_LSQR numeric TOK_RSQR TOK_LBRACE mappingList TOK_RBRACE { $$ = new CMappingDeclaration(*$2,*$4,*$7,&@6,&@8); } 
	     ;

pin_type: TOK_IN
	| TOK_OUT
	| TOK_BIDIRECTIONAL
	;

var_decl : TOK_DECLARE TOK_INTERNAL ident TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(CVariableDeclaration::notArray,true, *$3, *$5,CombineTokenLocations(@1,@6)); }
	| TOK_DECLARE TOK_INTERNAL ident TOK_LSQR numeric TOK_RSQR alias_list { $$ = new CVariableDeclaration(CVariableDeclaration::notArray,true, *$3, *$5, *$7,CombineTokenLocations(@1,@6)); delete $7; }
	| TOK_DECLARE ident TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(CVariableDeclaration::notArray,false, *$2, *$4,CombineTokenLocations(@1,@5)); }
	| TOK_DECLARE ident TOK_LSQR numeric TOK_RSQR alias_list { $$ = new CVariableDeclaration(CVariableDeclaration::notArray,false, *$2, *$4, *$6,CombineTokenLocations(@1,@5)); delete $6; }
	| TOK_DECLARE TOK_INTERNAL ident TOK_INDEXOPEN numeric TOK_INDEXCLOSE TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(*$5,true, *$3, *$8,CombineTokenLocations(@1,@9)); }
	| TOK_DECLARE TOK_INTERNAL ident TOK_INDEXOPEN numeric TOK_INDEXCLOSE TOK_LSQR numeric TOK_RSQR alias_list { $$ = new CVariableDeclaration(*$5, true, *$3, *$8, *$10,CombineTokenLocations(@1,@9)); delete $10; }
	| TOK_DECLARE ident TOK_INDEXOPEN numeric TOK_INDEXCLOSE TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(*$4, false, *$2, *$7,CombineTokenLocations(@1,@8)); }
	| TOK_DECLARE ident TOK_INDEXOPEN numeric TOK_INDEXCLOSE TOK_LSQR numeric TOK_RSQR alias_list { $$ = new CVariableDeclaration(*$4, false, *$2, *$7, *$9,CombineTokenLocations(@1,@8)); delete $9; }
	| TOK_PIN pin_type ident TOK_LSQR numeric TOK_RSQR { $$ = new CVariableDeclaration(*$3,*$5,$2,CombineTokenLocations(@1,@6)); }
	| TOK_PIN pin_type ident TOK_LSQR numeric TOK_RSQR alias_list { $$ = new CVariableDeclaration(*$3,*$5,*$7,$2,CombineTokenLocations(@1,@6)); delete $7; }
	;

affector : ident TOK_AS TOK_ZERO { $$ = new CAffect(*$1,TOK_ZERO); }
	 | ident TOK_AS TOK_NONZERO { $$ = new CAffect(*$1,TOK_NONZERO); }
	 | ident TOK_AS TOK_SIGN { $$ = new CAffect(*$1,TOK_SIGN); }
	 | ident TOK_AS TOK_NOSIGN { $$ = new CAffect(*$1,TOK_NOSIGN); }
	 | ident TOK_AS TOK_FORCESET { $$ = new CAffect(*$1,TOK_FORCESET); }
	 | ident TOK_AS TOK_FORCERESET { $$ = new CAffect(*$1,TOK_FORCERESET); }
	 | ident TOK_AS TOK_PARITYEVEN { $$ = new CAffect(*$1,TOK_PARITYEVEN); }
	 | ident TOK_AS TOK_PARITYODD { $$ = new CAffect(*$1,TOK_PARITYODD); }
	 | ident TOK_AS TOK_BIT TOK_OBR numeric TOK_CBR { $$ = new CAffect(*$1,TOK_BIT,*$5); }
	 | ident TOK_AS TOK_INVBIT TOK_OBR numeric TOK_CBR { $$ = new CAffect(*$1,TOK_INVBIT,*$5); }
	 | ident TOK_AS TOK_CARRY TOK_OBR numeric TOK_CBR { $$ = new CAffect(*$1,TOK_CARRY,*$5); }
	 | ident TOK_AS TOK_NOCARRY TOK_OBR numeric TOK_CBR { $$ = new CAffect(*$1,TOK_NOCARRY,*$5); }
	 | ident TOK_AS TOK_OVERFLOW TOK_OBR expr TOK_COMMA expr TOK_COMMA numeric TOK_CBR { $$ = new CAffect(*$1,TOK_OVERFLOW,*$5,*$7,*$9); }
	 | ident TOK_AS TOK_NOOVERFLOW TOK_OBR expr TOK_COMMA expr TOK_COMMA numeric TOK_CBR { $$ = new CAffect(*$1,TOK_NOOVERFLOW,*$5,*$7,*$9); }
	 ;

affectors : affectors TOK_COMMA affector { $$->push_back($<affect>3); }
	  | affector { $$ = new AffectorList(); $$->push_back($<affect>1); }
	;

params : params TOK_COMMA expr	{ $$->push_back($3); }
       | expr			{ $$ = new ParamsList(); $$->push_back($1); }
	|			{ $$ = new ParamsList(); }
	;

expr : ident_ref TOK_ASSIGNLEFT expr { $$ = new CAssignment(*$<ident>1,*$3, &@2); }
     | expr TOK_ASSIGNRIGHT ident_ref { $$ = new CAssignment(*$<ident>3,*$1, &@2); }
     | expr TOK_ADD expr { $$ = new CBinaryOperator(*$1,TOK_ADD,*$3, &@2); }
     | expr TOK_SUB expr { $$ = new CBinaryOperator(*$1,TOK_SUB,*$3, &@2); }
     | expr TOK_MUL expr { $$ = new CBinaryOperator(*$1,TOK_MUL,*$3, &@2); }
     | expr TOK_DIV expr { $$ = new CBinaryOperator(*$1,TOK_DIV,*$3, &@2); }
     | expr TOK_MOD expr { $$ = new CBinaryOperator(*$1,TOK_MOD,*$3, &@2); }
     | expr TOK_SDIV expr { $$ = new CBinaryOperator(*$1,TOK_SDIV,*$3, &@2); }
     | expr TOK_SMOD expr { $$ = new CBinaryOperator(*$1,TOK_SMOD,*$3, &@2); }
     | expr TOK_DADD expr { $$ = new CBinaryOperator(*$1,TOK_DADD,*$3, &@2); }
     | expr TOK_DSUB expr { $$ = new CBinaryOperator(*$1,TOK_DSUB,*$3, &@2); }
     | expr TOK_CMPEQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPEQ,*$3, &@2); }
     | expr TOK_CMPNEQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPNEQ,*$3, &@2); }
     | expr TOK_CMPLESSEQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPLESSEQ,*$3, &@2); }
     | expr TOK_CMPLESS expr { $$ = new CBinaryOperator(*$1,TOK_CMPLESS,*$3, &@2); }
     | expr TOK_CMPGREATEREQ expr { $$ = new CBinaryOperator(*$1,TOK_CMPGREATEREQ,*$3, &@2); }
     | expr TOK_CMPGREATER expr { $$ = new CBinaryOperator(*$1,TOK_CMPGREATER,*$3, &@2); }
     | expr TOK_BAR expr { $$ = new CBinaryOperator(*$1,TOK_BAR,*$3, &@2); }
     | expr TOK_AMP expr { $$ = new CBinaryOperator(*$1,TOK_AMP,*$3, &@2); }
     | expr TOK_HAT expr { $$ = new CBinaryOperator(*$1,TOK_HAT,*$3, &@2); }
     | state_ident_list TOK_AT { $$ = new CStateTest(*$1); }
     | TOK_TILDE expr { $$ = new CBinaryOperator(*$2,TOK_TILDE,*$2, &@1); }
     | TOK_ROL TOK_OBR expr TOK_COMMA ident_ref TOK_COMMA expr TOK_COMMA expr TOK_CBR { $$ = new CRotationOperator(TOK_ROL,*$3,*$5,*$7,*$9); }
     | TOK_ROR TOK_OBR expr TOK_COMMA ident_ref TOK_COMMA expr TOK_COMMA expr TOK_CBR { $$ = new CRotationOperator(TOK_ROR,*$3,*$5,*$7,*$9); }
     | expr TOK_LSQR numeric TOK_RSQR { $3->Decrement(); $$ = new CCastOperator(*$1,*$3,CombineTokenLocations(@2,@4)); }
     | expr TOK_LSQR numeric TOK_DDOT numeric TOK_RSQR { $$ = new CCastOperator(*$1,*$3,*$5,CombineTokenLocations(@2,@6)); }
     | TOK_CALL ident_ref TOK_OBR params TOK_CBR { $$ = new CFuncCall(*$2,*$4,&@1); }
     | TOK_AFFECT affectors TOK_LBRACE expr TOK_RBRACE { $$ = new CAffector(*$2,*$4,CombineTokenLocations(@3,@5)); }
     | ident_ref { $<ident>$ = $1; }
     | numeric { $$ = $1; }
     | TOK_HIGH_IMPEDANCE { $$ = new CHighImpedance(); }
     | TOK_OBR expr TOK_CBR { $$ = $2; }
	;

quoted : TOK_STRING { $$ = new CString(*$1,&@1); delete $1; }
       ;

ident : TOK_IDENTIFIER { $$ = new CIdentifier(*$1, &@1); delete $1; }
	  ;

ident_ref : TOK_IDENTIFIER { $$ = new CIdentifier(*$1, &@1); }
	  | TOK_IDENTIFIER TOK_INDEXOPEN expr TOK_INDEXCLOSE { $$ = new CIdentifierArray(*$3,*$1,&@1); }
	  | TOK_IDENTIFIER TOK_IDENTIFIER { $$ = new CIdentifier(*$1,*$2,&@1,&@2); }
	  ;

numeric : TOK_INTEGER { $$ = new CInteger(*$1,&@1); delete $1; }
		;

%%
