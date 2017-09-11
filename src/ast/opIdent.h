#pragma once

class COperandIdent : public COperand 
{
private:
	CIdentifier& ident;
	CInteger& size;

public:
	COperandIdent(CIdentifier& ident,CInteger& size) : ident(ident), size(size) { }
	
	virtual void DeclareLocal(CodeGenContext& context,unsigned num);
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num)  const
	{ 
		return llvm::APInt((unsigned int)size.getAPInt().getLimitedValue(),(uint64_t)num,false); 
	}
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) const
	{
		return 1<<size.getAPInt().getLimitedValue(); 
	}
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) const 
	{ 
		return nullptr; 
	};
};
