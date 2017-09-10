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

class CNode 
{
public:
	virtual ~CNode() {}

	virtual void prePass(CodeGenContext& context) { }
	virtual llvm::Value* codeGen(CodeGenContext& context) { return nullptr; }
};

class CExpression : public CNode 
{
public:
	virtual bool IsAssignmentExpression() const { return false; }
	virtual bool IsLeaf() const { return true; }
	virtual bool IsIdentifierExpression() const { return false; }
	virtual bool IsCarryExpression() const { return false; }
	virtual bool IsImpedance() const { return false; }
};

class CStatement : public CNode 
{
};

class COperand : public CNode 
{
public:
	virtual void DeclareLocal(CodeGenContext& context,unsigned num)=0;
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const=0;
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const=0;
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const=0;

	virtual bool isStringReturnable() const { return false; }
};

#include "ast/integer.h"

class CHighImpedance : public CExpression 
{
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

	COperandNumber(CInteger& integer) : integer(integer) { }
	
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

class CMapping : public CExpression 
{
public:
	CExpression&	expr;
	CInteger&	selector;
	CString&	label;

	CMapping(CInteger& selector,CString& label,CExpression& expr) : expr(expr), selector(selector), label(label) { }
	
	virtual bool IsLeaf() const { return false; }
};

#include "ast/opAssignment.h"
#include "ast/block.h"
#include "ast/debugHelpers.h"

class CExpressionStatement : public CStatement 
{
public:
	CExpression& expression;

	CExpressionStatement(CExpression& expression) : expression(expression) { }

	virtual void prePass(CodeGenContext& context) { expression.prePass(context); }
	virtual llvm::Value* codeGen(CodeGenContext& context) { return expression.codeGen(context); }
};

#include "ast/variableDecl.h"

class CAliasDeclaration : public CStatement 
{
public:
	CIdentifier& idOrEmpty;
	CInteger& sizeOrValue;
	static CIdentifier empty;

	CAliasDeclaration(CIdentifier& id, CInteger& size) : idOrEmpty(id), sizeOrValue(size) { }
	CAliasDeclaration(CInteger& value) : idOrEmpty(empty), sizeOrValue(value) { }
};

#include "ast/stateDecl.h"
#include "ast/statesDecl.h"
#include "ast/stateTest.h"
#include "ast/stateJump.h"
#include "ast/statePush.h"
#include "ast/statePop.h"
#include "ast/ifStatement.h"
#include "ast/stateDefinition.h"
#include "ast/trigger.h"
#include "ast/handlerDecl.h"

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

#include "ast/connectDecl.h"
#include "ast/mappingDecl.h"
#include "ast/instruction.h"
#include "ast/execute.h"
#include "ast/affect.h"
#include "ast/affector.h"
#include "ast/extern.h"
#include "ast/funcDecl.h"
#include "ast/funcCall.h"

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

