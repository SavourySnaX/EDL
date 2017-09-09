#pragma once

class CInstruction : public CStatement 
{
private:

	CString& processMnemonic(CodeGenContext& context, CString& in, llvm::APInt& opcode, unsigned num) const;
	std::string EscapeString(const std::string &in) const;

public:
	CIdentifier& table;
	CString& mnemonic;
	OperandList operands;
	CBlock& block;
	static CIdentifier emptyTable;
	YYLTYPE statementLoc;

	CInstruction(CIdentifier& table, CString& mnemonic, OperandList& operands, CBlock& block, YYLTYPE *statementLoc) : table(table), mnemonic(mnemonic), operands(operands), block(block), statementLoc(*statementLoc) { }
	CInstruction(CString& mnemonic, OperandList& operands, CBlock& block, YYLTYPE *statementLoc) : table(emptyTable), mnemonic(mnemonic), operands(operands), block(block), statementLoc(*statementLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
