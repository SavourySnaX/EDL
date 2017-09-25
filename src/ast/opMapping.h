#pragma once

class COperandMapping : public COperand 
{
public:
	CIdentifier& ident;
	COperandMapping(CIdentifier& ident) : ident(ident) { }

	virtual void DeclareLocal(CodeGenContext& context,unsigned num);
	virtual bool IsFunctionArgument() const { return true; }
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) const;
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const;
	virtual void GetArgTypes(CodeGenContext& context, std::vector<llvm::Type*>& argVector) const;
	virtual void GetArgs(CodeGenContext& context, std::vector<llvm::Value*>& argVector,unsigned num) const;
	
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const;

	virtual bool isStringReturnable() const 
	{
		return true; 
	}
};
