#pragma once

class CConnectDeclaration : public CStatement
{
public:
	ConnectList connects;
	const CIdentifier& ident;
	YYLTYPE statementLoc;
	YYLTYPE blockStartLoc;
	YYLTYPE blockEndLoc;

	CConnectDeclaration(const CIdentifier& ident, ConnectList& connects, YYLTYPE *statementLoc, YYLTYPE *blockStartLoc, YYLTYPE *blockEndLoc) :
		connects(connects), ident(ident), statementLoc(*statementLoc), blockStartLoc(*blockStartLoc), blockEndLoc(*blockEndLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
