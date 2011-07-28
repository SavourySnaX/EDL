#include <iostream>
#include <vector>
#include <llvm/Value.h>
#include <llvm/Function.h>
#include <llvm/ADT/APInt.h>
#include <llvm/BasicBlock.h>

class CodeGenContext;
class CStatement;
class CExpression;
class CVariableDeclaration;
class CStateDeclaration;
class CAliasDeclaration;
class CDebugTrace;
class CStateIdent;

typedef std::vector<CStatement*> StatementList;
typedef std::vector<CExpression*> ExpressionList;
typedef std::vector<CVariableDeclaration*> VariableList;
typedef std::vector<CStateDeclaration*> StateList;
typedef std::vector<CAliasDeclaration*> AliasList;
typedef std::vector<CDebugTrace*> DebugList;
typedef std::vector<CStateIdent*> StateIdentList;

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
		if (radix==10)
		{
			if (integer.getActiveBits())						// this shrinks the value to the correct number of bits - fixes over allocation for decimal numbers
				integer = integer.trunc(integer.getActiveBits());		// only performed on decimal numbers (otherwise we loose important leading zeros)
			else
				integer = integer.trunc(1);
		}
	}
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CIdentifier : public CExpression {
public:
	std::string name;
	CIdentifier(const std::string& name) : name(name) { }
	llvm::Value* trueSize(llvm::Value*,CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateIdent : public CExpression {
public:
	std::string name;
	CStateIdent(const std::string& name) : name(name) { }
	virtual llvm::Value* codeGen(CodeGenContext& context) { }
};

class CString : public CExpression {
public:
	std::string quoted;
	CString(const std::string& quoted) : quoted(quoted) { }
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

class CDebugTrace : public CStatement
{
public:
	int currentBase;
	virtual bool isModifier() { return false; }
};

class CDebugLine : public CStatement
{
public:
	DebugList debug;
	CDebugLine(DebugList& debug) :
		debug(debug) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceString : public CDebugTrace {
public:
	CString& string;
	CDebugTraceString(CString& string) :
		string(string) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceInteger : public CDebugTrace {
public:
	CInteger& integer;
	CDebugTraceInteger(CInteger& integer) :
		integer(integer) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceIdentifier : public CDebugTrace {
public:
	CIdentifier& ident;
	CDebugTraceIdentifier(CIdentifier& ident) :
		ident(ident) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceBase : public CDebugTrace {
public:
	CInteger& integer;
	CDebugTraceBase(CInteger& integer) :
		integer(integer) { }
	virtual bool isModifier() { return true; }
	virtual llvm::Value* codeGen(CodeGenContext& context) {return NULL; }
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
	AliasList aliases;
	CVariableDeclaration(CIdentifier& id, CInteger& size) :
		id(id), size(size) { }
	CVariableDeclaration(CIdentifier& id, CInteger& size,AliasList& aliases) :
		id(id), size(size),aliases(aliases) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CAliasDeclaration : public CStatement {
public:
	CIdentifier& idOrEmpty;
	CInteger& sizeOrValue;
	static CIdentifier empty;
	CAliasDeclaration(CIdentifier& id, CInteger& size) :
		idOrEmpty(id), sizeOrValue(size) { }
	CAliasDeclaration(CInteger& value) :
		idOrEmpty(empty), sizeOrValue(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateDeclaration : public CStatement {
public:
	CIdentifier& id;
	bool autoIncrement;
	llvm::BasicBlock* entry;
	llvm::BasicBlock* stateAdjust;
	llvm::BasicBlock* exit;
	CStateDeclaration(CIdentifier& id) :
		id(id),autoIncrement(false),entry(NULL),stateAdjust(NULL),exit(NULL) { }
	virtual llvm::Value* codeGen(CodeGenContext& context,llvm::Function* parent);
};

class CStatesDeclaration : public CStatement {
public:
	StateList states;
	std::string label;
	llvm::BasicBlock* exitState;
	CBlock& block;
	CStatesDeclaration(StateList& states,CBlock& block) :
		states(states),block(block) { }

	CStateDeclaration* getStateDeclaration(const CIdentifier& id) { for (int a=0;a<states.size();a++) { if (states[a]->id.name == id.name) return states[a]; } return NULL; }
	int getStateDeclarationIndex(const CIdentifier& id) { for (int a=0;a<states.size();a++) { if (states[a]->id.name == id.name) return a; } return -1; }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateTest : public CExpression {
public:
	StateIdentList stateIdents;
	CStateTest(StateIdentList& stateIdents) :
		stateIdents(stateIdents) { }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CIfStatement : public CStatement
{
public:
	CExpression& expr;
	CBlock& block;
	CIfStatement(CExpression& expr, CBlock& block) :
		expr(expr),block(block) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateDefinition : public CStatement {
public:
	const CIdentifier& id;
	CBlock& block;
	CStateDefinition(const CIdentifier& id, CBlock& block) :
		id(id), block(block) { }
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
