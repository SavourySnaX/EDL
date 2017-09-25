#pragma once

class COperandPartial : public COperand 
{
public:
	OperandList operands;

	COperandPartial() {}

	void Add(COperand* operand) { operands.push_back(operand); }

	virtual bool IsFunctionArgument() const { return false; }
	virtual void DeclareLocal(CodeGenContext& context,unsigned num);

	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const;
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const;
	virtual void GetArgTypes(CodeGenContext& context, std::vector<llvm::Type*>& argVector) const;
	virtual void GetArgs(CodeGenContext& context, std::vector<llvm::Value*>& argVector,unsigned num) const;
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const;
	
	void SetupFunctionArgs(CodeGenContext& context, llvm::Function* func);

	virtual bool isStringReturnable() const;
};

