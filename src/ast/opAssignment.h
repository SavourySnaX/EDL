#pragma once

class CAssignment : public CExpression
{
public:
	CIdentifier& lhs;
	CExpression& rhs;
	YYLTYPE operatorLoc;

	CAssignment(CIdentifier& lhs, CExpression& rhs, YYLTYPE* operatorLoc) : lhs(lhs), rhs(rhs), operatorLoc(*operatorLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context, CCastOperator* cast);

	virtual bool IsLeaf() const { return false; }
	virtual bool IsAssignmentExpression() const { return true; }

	static llvm::Instruction* generateAssignment(BitVariable& to, const CIdentifier& identTo, llvm::Value* from, CodeGenContext& context, bool isVolatile = false);
	static llvm::Instruction* generateAssignmentActual(BitVariable& to, const CIdentifier& identTo, llvm::Value* from, CodeGenContext& context, bool clearImpedance, bool isVolatile = false);
	static llvm::Instruction* generateImpedanceAssignment(BitVariable& to, llvm::Value* assignTo, CodeGenContext& context);
};
