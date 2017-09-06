#pragma once

class CBlock : public CExpression
{
public:
	StatementList statements;
	YYLTYPE blockStartLoc;
	YYLTYPE blockEndLoc;

	CBlock() { }

	void SetBlockLocation(YYLTYPE *start, YYLTYPE *end)
	{
		blockStartLoc = *start;
		blockEndLoc = *end;
	}

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
