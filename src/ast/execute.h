#pragma once

class CExecute : public CStatement
{
public:
	CIdentifier& table;
	CIdentifier& opcode;
	static CIdentifier emptyTable;
	YYLTYPE executeLoc;

	CExecute(CIdentifier& table, CIdentifier& opcode, YYLTYPE* executeLoc) : table(table), opcode(opcode), executeLoc(*executeLoc) { }
	CExecute(CIdentifier& opcode, YYLTYPE* executeLoc) : table(emptyTable), opcode(opcode), executeLoc(*executeLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

