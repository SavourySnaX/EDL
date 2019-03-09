#pragma once

class CConnectDeclaration : public CStatement
{
public:
	ConnectList connects;
	IdentifierList inputsToSchematic;
	YYLTYPE statementLoc;
	YYLTYPE blockStartLoc;
	YYLTYPE blockEndLoc;

	CConnectDeclaration(IdentifierList& inputs, ConnectList& connects, YYLTYPE *statementLoc, YYLTYPE *blockStartLoc, YYLTYPE *blockEndLoc) :
		connects(connects), inputsToSchematic(inputs), statementLoc(*statementLoc), blockStartLoc(*blockStartLoc), blockEndLoc(*blockEndLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
