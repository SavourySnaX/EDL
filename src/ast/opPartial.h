#pragma once

class COperandPartial : public COperand 
{
public:
	OperandList operands;

	COperandPartial() {}

	void Add(COperand *operand) { operands.push_back(operand); }

	virtual void DeclareLocal(CodeGenContext& context,unsigned num);

	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const;
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const;
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const;
	
	virtual bool isStringReturnable() const;
};

