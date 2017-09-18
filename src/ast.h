#pragma once

#include <iostream>
#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/BasicBlock.h>


#define MAX_SUPPORTED_STACK_DEPTH	(256)
#define MAX_SUPPORTED_STACK_BITS	(8)

enum ConnectionType
{
	None,
	Pullup
};

struct YYLTYPE;

#include "nodes.h"
#include "ast/integer.h"

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

#include "ast/string.h"

class COperandNumber : public COperand 
{
public:
	CInteger& integer;

	COperandNumber(CInteger& integer) : integer(integer) { }
	
	virtual void DeclareLocal(CodeGenContext& context,unsigned num) {}
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const { return integer.getAPInt(); }
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

#include "ast/instance.h"
#include "ast/exchange.h"
