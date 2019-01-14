#pragma once

class CExecute : public CStatement
{
public:
	CIdentifier& table;
	CIdentifier& opcode;
	static CIdentifier emptyTable;
	YYLTYPE executeLoc;
    int tableOffset;

	CExecute(CIdentifier& table, CIdentifier& opcode, YYLTYPE* executeLoc) : table(table), opcode(opcode), executeLoc(*executeLoc),tableOffset(-1) { }
	CExecute(CIdentifier& opcode, YYLTYPE* executeLoc) : table(emptyTable), opcode(opcode), executeLoc(*executeLoc),tableOffset(-1) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);

    void LinkToSwitch(CodeGenContext& context, llvm::Function* func, std::vector<llvm::Value*> args, std::string name, llvm::APInt& caseNum);
};

