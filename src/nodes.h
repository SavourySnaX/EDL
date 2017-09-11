#pragma once

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Value.h>

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
