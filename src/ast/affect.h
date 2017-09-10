#pragma once

class CAffect : public CExpression 
{
public:
	const CIdentifier& ident;
	int type;
	int opType;
	CInteger& param;
	CExpression& ov1;
	CExpression& ov2;
	llvm::Value* tmpResult;
	llvm::Value* ov1Val;
	llvm::Value* ov2Val;
	static CInteger emptyParam;

	CAffect(CIdentifier& ident, int type) : ident(ident), type(type), param(emptyParam), ov1(ident), ov2(ident) { }
	CAffect(CIdentifier& ident, int type, CInteger& param) : ident(ident), type(type), param(param), ov1(ident), ov2(ident) { }
	CAffect(CIdentifier& ident, int type, CExpression& ov1, CExpression& ov2, CInteger& param) : ident(ident), type(type), opType(type), param(param), ov1(ov1), ov2(ov2) { }

	llvm::Value* codeGenCarry(CodeGenContext& context, llvm::Value* exprResult, llvm::Value* lhs, llvm::Value* rhs, int type);
	llvm::Value* codeGenFinal(CodeGenContext& context, llvm::Value* exprResult);
};
