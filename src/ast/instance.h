#pragma once

class CInstance : public CStatement 
{
public:
	CString&	filename;
	CIdentifier&	ident;

	CInstance(CString& filename, CIdentifier& ident) : filename(filename), ident(ident) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
