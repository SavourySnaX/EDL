#pragma once

class CString : public CExpression
{
public:
	std::string quoted;
	YYLTYPE quotedLoc;
	CString(const std::string& quoted) : quoted(quoted) { }
	CString(const std::string& quoted, YYLTYPE *quotedLoc) : quoted(quoted), quotedLoc(*quotedLoc) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

