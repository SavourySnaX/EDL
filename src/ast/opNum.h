#pragma once

class COperandNumber : public COperand 
{
public:
	CInteger& integer;

	COperandNumber(CInteger& integer) : integer(integer) { }
	
	virtual void DeclareLocal(CodeGenContext& context,unsigned num) {}
	virtual bool IsFunctionArgument() const { return false; }
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const { return integer.getAPInt(); }
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const { return 1; }
	virtual void GetArgTypes(CodeGenContext& context, std::vector<llvm::Type*>& argVector) const;
	virtual void GetArgs(CodeGenContext& context, std::vector<llvm::Value*>& argVector,unsigned num) const;
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const { return nullptr; };
};
