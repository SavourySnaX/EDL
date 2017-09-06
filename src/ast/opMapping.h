#pragma once

class COperandMapping : public COperand 
{
public:
	CIdentifier& ident;
	COperandMapping(CIdentifier& ident) : ident(ident) { }

	virtual void DeclareLocal(CodeGenContext& context,unsigned num);
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const;
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const;
	
	BitVariable GetBitVariable(CodeGenContext& context,unsigned num);
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const;

	virtual bool isStringReturnable() const 
	{
		return true; 
	}
};
