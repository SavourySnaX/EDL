#pragma once

class CTrigger : public CExpression
{
private:
	llvm::Value* GenerateAlways(CodeGenContext& context, BitVariable& pin, llvm::FunctionType* ftype, llvm::Value* function);
	llvm::Value* GenerateChanged(CodeGenContext& context, BitVariable& pin, llvm::FunctionType* ftype, llvm::Value* function);
	llvm::Value* GenerateTransition(CodeGenContext& context, BitVariable& pin, llvm::FunctionType* ftype, llvm::Value* function);
public:
	int type;
	CInteger& param1;
	CInteger& param2;
	YYLTYPE debugLocation;
	static CInteger zero;

	CTrigger(int type, YYLTYPE* debugLocation) : type(type), debugLocation(*debugLocation), param1(zero), param2(zero) { }
	CTrigger(int type, YYLTYPE* debugLocation, CInteger& param1, CInteger& param2) : type(type), debugLocation(*debugLocation), param1(param1), param2(param2) { }

	virtual llvm::Value* codeGen(CodeGenContext& context, BitVariable& pin, llvm::FunctionType* funcType, llvm::Value* function);
};
