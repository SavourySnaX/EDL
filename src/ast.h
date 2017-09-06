#include <iostream>
#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/BasicBlock.h>


#define MAX_SUPPORTED_STACK_DEPTH	(256)
#define MAX_SUPPORTED_STACK_BITS	(8)

extern llvm::LLVMContext TheContext;

enum ConnectionType
{
	None,
	Pullup
};

struct YYLTYPE;

class BitVariable;

class CodeGenContext;
class CStatement;
class CExpression;
class CVariableDeclaration;
class CStateDeclaration;
class CStatesDeclaration;
class CAliasDeclaration;
class CDebugTrace;
class CStateIdent;
class COperand;
class CMapping;
class CAffect;
class CString;
class CInteger;
class CParamDecl;
class CConnect;

typedef std::vector<CStatement*> StatementList;
typedef std::vector<CExpression*> ExpressionList;
typedef std::vector<CVariableDeclaration*> VariableList;
typedef std::vector<CStateDeclaration*> StateList;
typedef std::vector<CStatesDeclaration*> StatesList;
typedef std::vector<CAliasDeclaration*> AliasList;
typedef std::vector<CDebugTrace*> DebugList;
typedef std::vector<CStateIdent*> StateIdentList;
typedef std::vector<COperand*> OperandList;
typedef std::vector<CMapping*> MappingList;
typedef std::vector<CAffect*> AffectorList;
typedef std::vector<CInteger*> ExternParamsList;
typedef std::vector<CExpression*> ParamsList;
typedef std::vector<CParamDecl*> NamedParamsList;
typedef std::vector<CConnect*> ConnectList;

class CNode {
public:
	virtual ~CNode() {}
	virtual void prePass(CodeGenContext& context) { }
	virtual llvm::Value* codeGen(CodeGenContext& context) { return nullptr; }
};

class CExpression : public CNode {
public:
	virtual bool IsAssignmentExpression() const { return false; }
	virtual bool IsLeaf() const { return true; }
	virtual bool IsIdentifierExpression() const { return false; }
	virtual bool IsCarryExpression() const { return false; }
	virtual bool IsImpedance() const { return false; }
};

class CStatement : public CNode {
};

class COperand : public CNode {
public:
	virtual void DeclareLocal(CodeGenContext& context,unsigned num)=0;
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const=0;
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const=0;
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const=0;

	virtual bool isStringReturnable() const { return false; }
};

#include "ast/integer.h"

class CHighImpedance : public CExpression {
public:

	CHighImpedance(){};
	
	virtual bool IsImpedance() const { return true; }

	virtual llvm::Value* codeGen(CodeGenContext& context) { return nullptr; } 
};

class CBaseIdentifier : public CExpression
{
public:
	virtual CExpression *GetExpression() const=0;
	virtual bool IsArray() const { return false;}
};

#include "ast/identifier.h"

class CIdentifierArray : public CIdentifier
{
public:
	CExpression& arrayIndex;
	CIdentifierArray(CExpression& expr,const std::string& name): CIdentifier(name),arrayIndex(expr) {};
	CIdentifierArray(CExpression& expr,const std::string& name, YYLTYPE *nameLoc): CIdentifier(name,nameLoc),arrayIndex(expr) {};
	CIdentifierArray(CExpression& expr,const std::string& module, const std::string& name,YYLTYPE *modLoc, YYLTYPE *nameLoc): CIdentifier(module,name,modLoc,nameLoc),arrayIndex(expr) {};
	virtual CExpression *GetExpression() const { return &arrayIndex; }
	virtual bool IsArray() const { return true; }
};

class CStateIdent : public CExpression {
public:
	std::string name;
	YYLTYPE nameLoc;
	CStateIdent(const std::string& name, YYLTYPE *nameLoc) : name(name),nameLoc(*nameLoc) { }
};

#include "ast/string.h"

class COperandNumber : public COperand {
public:
	CInteger& integer;
	COperandNumber(CInteger& integer) :
		integer(integer) { }
	
	virtual void DeclareLocal(CodeGenContext& context,unsigned num) {}
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const { return integer.integer; }
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const { return 1; }
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const { return nullptr; };
};

#include "ast/opIdent.h"
#include "ast/opMapping.h"
#include "ast/opPartial.h"
#include "ast/opBinary.h"
#include "ast/opCast.h"
#include "ast/opRotation.h"

class CMapping : public CExpression {
public:
	CExpression&	expr;
	CInteger&	selector;
	CString&	label;

	CMapping(CInteger& selector,CString& label,CExpression& expr) :
		expr(expr), selector(selector), label(label) { }
	
	virtual bool IsLeaf() const { return false; }
};

#include "ast/opAssignment.h"

class CBlock : public CExpression {
public:
	StatementList statements;
	YYLTYPE blockStartLoc;
	YYLTYPE blockEndLoc;
	CBlock() { }
	void SetBlockLocation(YYLTYPE *start, YYLTYPE *end)
	{
		blockStartLoc = *start;
		blockEndLoc = *end;
	}
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTrace : public CStatement
{
public:
	int currentBase;
	virtual bool isModifier() { return false; }
};

class CDebugLine : public CStatement
{
public:
	DebugList debug;
	YYLTYPE debugLocation;
	CDebugLine(DebugList& debug,YYLTYPE* debugLocation) :
		debug(debug),debugLocation(*debugLocation) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceString : public CDebugTrace {
public:
	CString& string;
	CDebugTraceString(CString& string) :
		string(string) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceInteger : public CDebugTrace {
public:
	CInteger& integer;
	CDebugTraceInteger(CInteger& integer) :
		integer(integer) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceIdentifier : public CDebugTrace {
public:
	CIdentifier& ident;
	CDebugTraceIdentifier(CIdentifier& ident) :
		ident(ident) { }
	llvm::Value* generate(CodeGenContext& context,llvm::Value* loadedValue);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceBase : public CDebugTrace {
public:
	CInteger& integer;
	CDebugTraceBase(CInteger& integer) :
		integer(integer) { }
	virtual bool isModifier() { return true; }
};

class CExpressionStatement : public CStatement {
public:
	CExpression& expression;
	CExpressionStatement(CExpression& expression) : 
		expression(expression) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CVariableDeclaration : public CStatement {
private:
	llvm::Instruction* writeAccessor;
public:
	static CInteger notArray;
	bool internal;
	CIdentifier& id;
	CInteger& arraySize;
	CInteger& size;
	AliasList aliases;
	int pinType;
	ExternParamsList initialiserList;
	YYLTYPE declarationLoc;

	CVariableDeclaration(CInteger& arraySize,bool internal,CIdentifier& id, CInteger& size, YYLTYPE declarationLoc) :
		internal(internal), id(id), arraySize(arraySize),size(size),pinType(0),declarationLoc(declarationLoc) { }
	CVariableDeclaration(CInteger& arraySize,bool internal,CIdentifier& id, CInteger& size,AliasList& aliases, YYLTYPE declarationLoc) :
		internal(internal), id(id), arraySize(arraySize),size(size),aliases(aliases),pinType(0),declarationLoc(declarationLoc) { }
	CVariableDeclaration(CIdentifier& id, CInteger& size,int pinType, YYLTYPE declarationLoc) :
		id(id), arraySize(notArray),size(size),pinType(pinType),declarationLoc(declarationLoc) { }
	CVariableDeclaration(CIdentifier& id, CInteger& size,AliasList& aliases,int pinType, YYLTYPE declarationLoc) :
		id(id), arraySize(notArray),size(size),aliases(aliases),pinType(pinType),declarationLoc(declarationLoc) { }

	void AddInitialisers(ExternParamsList &initialisers) {initialiserList=initialisers;}

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);

	void CreateWriteAccessor(CodeGenContext& context,BitVariable& var,const std::string& moduleName, const std::string& name,bool impedance);
	void CreateReadAccessor(CodeGenContext& context,BitVariable& var,bool impedance);
};

class CAliasDeclaration : public CStatement {
public:
	CIdentifier& idOrEmpty;
	CInteger& sizeOrValue;
	static CIdentifier empty;
	CAliasDeclaration(CIdentifier& id, CInteger& size) :
		idOrEmpty(id), sizeOrValue(size) { }
	CAliasDeclaration(CInteger& value) :
		idOrEmpty(empty), sizeOrValue(value) { }
};

class CStateDeclaration : public CStatement {
public:
	CIdentifier& id;
	bool autoIncrement;
	llvm::BasicBlock* entry;
	llvm::BasicBlock* exit;
	CStatesDeclaration* child;
	CStateDeclaration(CIdentifier& id) :
		id(id),autoIncrement(false),entry(nullptr),exit(nullptr),child(nullptr) { }
	virtual llvm::Value* codeGen(CodeGenContext& context,llvm::Function* parent);
};

class CStatesDeclaration : public CStatement {
public:
	StateList states;
	std::string label;
	StatesList children;
	llvm::BasicBlock* exitState;
	llvm::Value* optocurState;
	CBlock& block;
	YYLTYPE statementLoc;
	CStatesDeclaration(StateList& states,CBlock& block,YYLTYPE *statementLoc) :
		states(states),block(block),statementLoc(*statementLoc) { }

	CStateDeclaration* getStateDeclaration(const CIdentifier& id) { for (int a=0;a<states.size();a++) { if (states[a]->id.name == id.name) return states[a]; } return nullptr; }
	int getStateDeclarationIndex(const CIdentifier& id) { for (int a=0;a<states.size();a++) { if (states[a]->id.name == id.name) return a; } return -1; }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateTest : public CExpression {
public:
	StateIdentList stateIdents;
	CStateTest(StateIdentList& stateIdents) :
		stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateJump : public CStatement {
public:
	StateIdentList stateIdents;
	CStateJump(StateIdentList& stateIdents) :
		stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStatePush : public CStatement {
public:
	StateIdentList stateIdents;
	CStatePush(StateIdentList& stateIdents) :
		stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStatePop : public CStatement {
public:
	CIdentifier& ident;
	CStatePop(CIdentifier& ident) :
		ident(ident) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};


class CIfStatement : public CStatement
{
public:
	CExpression& expr;
	CBlock& block;
	CIfStatement(CExpression& expr, CBlock& block) :
		expr(expr),block(block) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateDefinition : public CStatement {
public:
	const CIdentifier& id;
	CBlock& block;
	CStateDefinition(const CIdentifier& id, CBlock& block) :
		id(id), block(block) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CTrigger : public CExpression {
public:
	int type;
	CInteger& param1;
	CInteger& param2;
	YYLTYPE debugLocation;
	static CInteger zero;
	CTrigger(int type,YYLTYPE* debugLocation) :
		type(type), debugLocation(*debugLocation),param1(zero),param2(zero) { }
	CTrigger(int type,YYLTYPE* debugLocation, CInteger& param1, CInteger& param2) :
		type(type), debugLocation(*debugLocation), param1(param1), param2(param2) { }
	virtual llvm::Value* codeGen(CodeGenContext& context,BitVariable& pin,llvm::Value* function);
};


class CHandlerDeclaration : public CStatement {
public:
	const CIdentifier& id;
	CTrigger& trigger;
	CBlock& block;
	llvm::GlobalVariable* depthTree;
	llvm::GlobalVariable* depthTreeIdx;
	CStatesDeclaration* child;
	YYLTYPE handlerLoc;
	CHandlerDeclaration(const CIdentifier& id, CTrigger& trigger,CBlock& block, YYLTYPE *handlerLoc) :
		id(id), trigger(trigger),block(block),handlerLoc(*handlerLoc) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CConnect{
public:
	ParamsList& connects;
	CString& tapName;
	CInteger& decodeWidth;
	bool hasTap;
	ConnectionType conType;
	YYLTYPE statementLoc;

	CConnect(ParamsList& connects, CString& tapName, CInteger& decodeWidth, ConnectionType conType, YYLTYPE *statementLoc) :
		connects(connects), tapName(tapName), decodeWidth(decodeWidth), hasTap(true), conType(conType), statementLoc(*statementLoc) { }
	CConnect(ParamsList& connects, ConnectionType conType, YYLTYPE *statementLoc) :
		connects(connects), tapName(*new CString("")),decodeWidth(*new CInteger(std::string("0"))),hasTap(false), conType(conType), statementLoc(*statementLoc) { }
};

class CConnectDeclaration : public CStatement {
public:
	ConnectList connects;
	const CIdentifier& ident;
	YYLTYPE statementLoc;
	YYLTYPE blockStartLoc;
	YYLTYPE blockEndLoc;

	CConnectDeclaration(const CIdentifier& ident, ConnectList& connects, YYLTYPE *statementLoc, YYLTYPE *blockStartLoc, YYLTYPE *blockEndLoc) :
		connects(connects), ident(ident),statementLoc(*statementLoc),blockStartLoc(*blockStartLoc),blockEndLoc(*blockEndLoc) { }
	
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CMappingDeclaration : public CStatement {
public:
	MappingList mappings;
	const CIdentifier& ident;
	CInteger& size;

	CMappingDeclaration(const CIdentifier& ident, CInteger& size, MappingList& mappings) :
		mappings(mappings), ident(ident), size(size) { }
	
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CInstruction : public CStatement {
public:
	CIdentifier& table;
	CString& mnemonic;
	OperandList operands;
	CBlock& block;
	static CIdentifier emptyTable;
	YYLTYPE statementLoc;

	CInstruction(CIdentifier& table,CString& mnemonic,OperandList& operands, CBlock& block, YYLTYPE *statementLoc) :
		table(table),mnemonic(mnemonic),operands(operands), block(block), statementLoc(*statementLoc) { }
	CInstruction(CString& mnemonic,OperandList& operands, CBlock& block, YYLTYPE *statementLoc) :
		table(emptyTable),mnemonic(mnemonic),operands(operands), block(block), statementLoc(*statementLoc) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);

	CString& processMnemonic(CodeGenContext& context,CString& in,llvm::APInt& opcode,unsigned num);
};

class CExecute : public CStatement {
public:
	CIdentifier& table;
	CIdentifier& opcode;
	static CIdentifier emptyTable;
	YYLTYPE executeLoc;
	CExecute(CIdentifier& table,CIdentifier& opcode,YYLTYPE* executeLoc) :
		table(table),opcode(opcode),executeLoc(*executeLoc) { }
	CExecute(CIdentifier& opcode, YYLTYPE* executeLoc) :
		table(emptyTable),opcode(opcode),executeLoc(*executeLoc) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CAffect : public CExpression {
public:
	const CIdentifier& ident;
	int type;
	int opType;
	CInteger& param;
	CExpression& ov1;
	CExpression& ov2;
	llvm::Value* tmpResult;
	llvm::Value* ov1Val;
	llvm::Value* ov2Val;
	static CInteger emptyParam;

	CAffect(CIdentifier& ident,int type) :
		ident(ident),type(type),param(emptyParam),ov1(ident),ov2(ident) { }
	CAffect(CIdentifier& ident,int type,CInteger& param) :
		ident(ident),type(type),param(param),ov1(ident),ov2(ident) { }
	CAffect(CIdentifier& ident,int type,CExpression& ov1,CExpression& ov2,CInteger& param) :
		ident(ident),type(type),opType(type),param(param),ov1(ov1),ov2(ov2) { }

	llvm::Value* codeGenCarry(CodeGenContext& context,llvm::Value* exprResult,llvm::Value* lhs,llvm::Value* rhs,int type);
	llvm::Value* codeGenFinal(CodeGenContext& context,llvm::Value* exprResult);
};

class CAffector : public CExpression {
public:
	AffectorList affectors;
	CExpression& expr;
	YYLTYPE exprLoc;
	
	CAffector(AffectorList& affectors,CExpression& expr, YYLTYPE exprLoc) :
		affectors(affectors),expr(expr),exprLoc(exprLoc) { }

	virtual void prePass(CodeGenContext& context);

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CExternDecl : public CStatement {
public:
	ExternParamsList returns;
	CIdentifier& name;
	ExternParamsList params;
	YYLTYPE declarationLoc;

	CExternDecl(ExternParamsList& returns,CIdentifier& name,ExternParamsList& params,YYLTYPE declarationLoc) :
		returns(returns), name(name), params(params), declarationLoc(declarationLoc) { }
	CExternDecl(CIdentifier& name,ExternParamsList& params,YYLTYPE declarationLoc) :
		name(name), params(params),declarationLoc(declarationLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CFuncCall : public CExpression {
public:
	CIdentifier& name;
	ParamsList& params;
	YYLTYPE callLoc;

	CFuncCall(CIdentifier& name,ParamsList& params,YYLTYPE* callLoc) :
		name(name), params(params),callLoc(*callLoc) { }
	
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CFunctionDecl : public CStatement {
public:
	bool internal;
	NamedParamsList returns;
	CIdentifier& name;
	NamedParamsList params;
	CBlock& block;
	YYLTYPE functionLoc;

	CFunctionDecl(bool internal,NamedParamsList& returns,CIdentifier& name,NamedParamsList& params,CBlock &block,YYLTYPE* functionLoc) :
		internal(internal), returns(returns), name(name), params(params),block(block),functionLoc(*functionLoc) { }
	CFunctionDecl(bool internal,CIdentifier& name,NamedParamsList& params,CBlock &block, YYLTYPE* functionLoc) :
		internal(internal), name(name), params(params),block(block),functionLoc(*functionLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CParamDecl : public CStatement {
public:
	CIdentifier& id;
	CInteger& size;
	CParamDecl(CIdentifier& id, CInteger& size) :
		id(id), size(size) { }
};

class CInstance : public CStatement {
public:
	CString&	filename;
	CIdentifier&	ident;

	CInstance(CString& filename,CIdentifier& ident) :
		filename(filename), ident(ident) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CExchange : public CStatement {
public:
	CIdentifier&	lhs;
	CIdentifier&	rhs;
	YYLTYPE operatorLoc;

	CExchange(CIdentifier& lhs,CIdentifier& rhs,YYLTYPE *operatorLoc) :
		lhs(lhs), rhs(rhs),operatorLoc(*operatorLoc) { }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

