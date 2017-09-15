#pragma once

#include <vector>

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Value.h>

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

class CodeGenContext;
class CString;

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
