#include <iostream>
#include <vector>
#include <llvm/Value.h>
#include <llvm/ADT/APInt.h>

class CodeGenContext;
class CStatement;
class CExpression;
class CVariableDeclaration;
class CStateDeclaration;

typedef std::vector<CStatement*> StatementList;
typedef std::vector<CExpression*> ExpressionList;
typedef std::vector<CVariableDeclaration*> VariableList;
typedef std::vector<CStateDeclaration*> StateList;

class CNode {
public:
	virtual ~CNode() {}
	virtual llvm::Value* codeGen(CodeGenContext& context) { }
};

class CExpression : public CNode {
};

class CStatement : public CNode {
};

class CInteger : public CExpression {
public:
	llvm::APInt	integer;

	CInteger(std::string& value ) 
	{
		unsigned char radix;
		const char* data;
		unsigned numBits;
		if (value[0]=='%')
		{
			numBits = value.length()-1;
			data = &value.c_str()[1];
			radix=2;
		}
		else
		{
			if (value[0]=='$')
			{
				numBits = 4*(value.length()-1);
				data = &value.c_str()[1];
				radix=16;
			}
			else
			{
				numBits = 4*(value.length());	/* over allocation for now */
				data = &value.c_str()[0];
				radix=10;
			}
		}

		integer = llvm::APInt(numBits,data,radix);
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CIdentifier : public CExpression {
public:
	std::string name;
	CIdentifier(const std::string& name) : name(name) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CMethodCall : public CExpression {
public:
	const CIdentifier& id;
	ExpressionList arguments;
	CMethodCall(const CIdentifier& id, ExpressionList& arguments) :
		id(id), arguments(arguments) { }
	CMethodCall(const CIdentifier& id) : id(id) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CBinaryOperator : public CExpression {
public:
	int op;
	CExpression& lhs;
	CExpression& rhs;
	CBinaryOperator(CExpression& lhs, int op, CExpression& rhs) :
		lhs(lhs), rhs(rhs), op(op) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CAssignment : public CExpression {
public:
	CIdentifier& lhs;
	CExpression& rhs;
	CAssignment(CIdentifier& lhs, CExpression& rhs) : 
		lhs(lhs), rhs(rhs) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CBlock : public CExpression {
public:
	StatementList statements;
	CBlock() { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CExpressionStatement : public CStatement {
public:
	CExpression& expression;
	CExpressionStatement(CExpression& expression) : 
		expression(expression) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CVariableDeclaration : public CStatement {
public:
	CIdentifier& id;
	CInteger& size;
	CVariableDeclaration(CIdentifier& id, CInteger& size) :
		id(id), size(size) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateDeclaration : public CStatement {
public:
	CIdentifier& id;
	CStateDeclaration(CIdentifier& id) :
		id(id) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStatesDeclaration : public CStatement {
public:
	StateList states;
	CStatesDeclaration(StateList& states) :
		states(states) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CHandlerDeclaration : public CStatement {
public:
	const CIdentifier& id;
	CBlock& block;
	CHandlerDeclaration(const CIdentifier& id, CBlock& block) :
		id(id), block(block) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
