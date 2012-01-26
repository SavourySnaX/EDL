#include <iostream>
#include <vector>
#include <llvm/Value.h>
#include <llvm/Function.h>
#include <llvm/ADT/APInt.h>
#include <llvm/BasicBlock.h>


#define MAX_SUPPORTED_STACK_DEPTH	(256)
#define MAX_SUPPORTED_STACK_BITS	(8)

class BitVariable;

class CodeGenContext;
class CStatement;
class CExpression;
class CVariableDeclaration;
class CStateDeclaration;
class CStatesDeclaration;
class CAliasDeclaration;
class CDebugTrace;
class CStateIdent;
class COperand;
class CMapping;
class CAffect;
class CString;
class CInteger;
class CParamDecl;

typedef std::vector<CStatement*> StatementList;
typedef std::vector<CExpression*> ExpressionList;
typedef std::vector<CVariableDeclaration*> VariableList;
typedef std::vector<CStateDeclaration*> StateList;
typedef std::vector<CStatesDeclaration*> StatesList;
typedef std::vector<CAliasDeclaration*> AliasList;
typedef std::vector<CDebugTrace*> DebugList;
typedef std::vector<CStateIdent*> StateIdentList;
typedef std::vector<COperand*> OperandList;
typedef std::vector<CMapping*> MappingList;
typedef std::vector<CAffect*> AffectorList;
typedef std::vector<CInteger*> ExternParamsList;
typedef std::vector<CExpression*> ParamsList;
typedef std::vector<CParamDecl*> NamedParamsList;

class CNode {
public:
	virtual ~CNode() {}
	virtual void prePass(CodeGenContext& context) { }
	virtual llvm::Value* codeGen(CodeGenContext& context) { return NULL; }
};

class CExpression : public CNode {
public:
	virtual bool IsAssignmentExpression() { return false; }
	virtual bool IsLeaf() { return true; }
	virtual bool IsIdentifierExpression() { return false; }
	virtual bool IsCarryExpression() { return false; }
};

class CStatement : public CNode {
};

class COperand : public CNode {
public:
	virtual void DeclareLocal(CodeGenContext& context,unsigned num)=0;
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num)=0;
	virtual unsigned GetNumComputableConstants(CodeGenContext& context)=0;
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot)=0;

	virtual bool isStringReturnable() { return false; }
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

	void Decrement()
	{
		integer--;
	}

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CBaseIdentifier : public CExpression
{
public:
	virtual CExpression *GetExpression() const=0;
	virtual bool IsArray() const { return false;}
};

class CIdentifier : public CBaseIdentifier {
public:
	std::string module;
	std::string name;
	CIdentifier(const std::string& name) : name(name) { }
	CIdentifier(const std::string& module, const std::string& name) : module(module),name(name) { }
	static llvm::Value* trueSize(llvm::Value*,CodeGenContext& context,BitVariable& var);
	static llvm::Value* GetAliasedData(CodeGenContext& context,llvm::Value* in,BitVariable& var);
	virtual llvm::Value* codeGen(CodeGenContext& context);
	virtual bool IsIdentifierExpression() { return true; }
	virtual CExpression *GetExpression() const { return NULL; }
};

class CIdentifierArray : public CIdentifier
{
public:
	CExpression& arrayIndex;
	CIdentifierArray(CExpression& expr,const std::string& name): CIdentifier(name),arrayIndex(expr) {};
	CIdentifierArray(CExpression& expr,const std::string& module, const std::string& name): CIdentifier(module,name),arrayIndex(expr) {};
	virtual CExpression *GetExpression() const { return &arrayIndex; }
	virtual bool IsArray() const { return true; }
};

class CStateIdent : public CExpression {
public:
	std::string name;
	CStateIdent(const std::string& name) : name(name) { }
};

class CString : public CExpression {
public:
	std::string quoted;
	CString(const std::string& quoted) : quoted(quoted) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class COperandNumber : public COperand {
public:
	CInteger& integer;
	COperandNumber(CInteger& integer) :
		integer(integer) { }
	
	virtual void DeclareLocal(CodeGenContext& context,unsigned num) {}
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) { return integer.integer; }
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) { return 1; }
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) { return NULL; };
};

class COperandIdent : public COperand {
public:
	CIdentifier& ident;
	CInteger& size;
	COperandIdent(CIdentifier& ident,CInteger& size) :
		ident(ident), size(size) { }
	
	virtual void DeclareLocal(CodeGenContext& context,unsigned num);
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num) { return llvm::APInt((unsigned int)size.integer.getLimitedValue(),(uint64_t)num,false); }
	virtual unsigned GetNumComputableConstants(CodeGenContext& context) { return 1<<size.integer.getLimitedValue(); }
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot) { return NULL; };
};

class COperandMapping : public COperand {
public:
	CIdentifier& ident;
	COperandMapping(CIdentifier& ident) :
		ident(ident) { }

	virtual void DeclareLocal(CodeGenContext& context,unsigned num);
	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num);
	virtual unsigned GetNumComputableConstants(CodeGenContext& context);
	
	BitVariable GetBitVariable(CodeGenContext& context,unsigned num);
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot);

	virtual bool isStringReturnable() { return true; }
};

class COperandPartial : public COperand {
public:
	OperandList operands;

	COperandPartial() {}

	void Add(COperand *operand) { operands.push_back(operand); }

	virtual void DeclareLocal(CodeGenContext& context,unsigned num);

	virtual llvm::APInt GetComputableConstant(CodeGenContext& context,unsigned num);
	virtual unsigned GetNumComputableConstants(CodeGenContext& context);
	virtual const CString* GetString(CodeGenContext& context,unsigned num,unsigned slot);
	
	virtual bool isStringReturnable();
};

class CBinaryOperator : public CExpression {
public:
	int op;
	CExpression& lhs;
	CExpression& rhs;
	CBinaryOperator(CExpression& lhs, int op, CExpression& rhs) :
		lhs(lhs), rhs(rhs), op(op) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
	llvm::Value* codeGen(CodeGenContext& context,llvm::Value* left,llvm::Value* right);

	virtual bool IsCarryExpression();
	
	virtual bool IsLeaf() { return false; }
};

class CCastOperator : public CExpression {
public:
	CExpression& lhs;
	CInteger& beg;
	CInteger& end;
	static CInteger begZero;
	CCastOperator(CExpression& lhs, CInteger& end) :
		lhs(lhs), beg(begZero),end(end) { }
	CCastOperator(CExpression& lhs, CInteger& beg, CInteger& end) :
		lhs(lhs), beg(beg), end(end) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
	
	virtual bool IsLeaf() { if (lhs.IsLeaf()) return true; return false; }
};

class CRotationOperator : public CExpression {
public:
	CExpression& value;
	CIdentifier& bitsOut;
	CExpression& bitsIn;
	CExpression& rotAmount;
	int direction;
	CRotationOperator(int direction,CExpression& value, CIdentifier& bitsOut, CExpression& bitsIn,CExpression& rotAmount) :
		direction(direction), value(value), bitsOut(bitsOut), bitsIn(bitsIn), rotAmount(rotAmount) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CMapping : public CExpression {
public:
	CExpression&	expr;
	CInteger&	selector;
	CString&	label;

	CMapping(CInteger& selector,CString& label,CExpression& expr) :
		expr(expr), selector(selector), label(label) { }
	
	virtual bool IsLeaf() { return false; }
};

class CAssignment : public CExpression {
public:
	CIdentifier& lhs;
	CExpression& rhs;
	CAssignment(CIdentifier& lhs, CExpression& rhs) : 
		lhs(lhs), rhs(rhs) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context,CCastOperator* cast);

	virtual bool IsLeaf() { return false; }
	virtual bool IsAssignmentExpression() { return true; }

	static llvm::Value* generateAssignment(BitVariable& to,const CIdentifier& identTo,llvm::Value* from,CodeGenContext& context);
};

class CBlock : public CExpression {
public:
	StatementList statements;
	CBlock() { }
	virtual void prePass(CodeGenContext& context);
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
	llvm::Value* generate(CodeGenContext& context,llvm::Value* loadedValue);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceBase : public CDebugTrace {
public:
	CInteger& integer;
	CDebugTraceBase(CInteger& integer) :
		integer(integer) { }
	virtual bool isModifier() { return true; }
};

class CExpressionStatement : public CStatement {
public:
	CExpression& expression;
	CExpressionStatement(CExpression& expression) : 
		expression(expression) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CVariableDeclaration : public CStatement {
private:
	llvm::Instruction* writeAccessor;
public:
	static CInteger notArray;
	bool internal;
	CIdentifier& id;
	CInteger& arraySize;
	CInteger& size;
	AliasList aliases;
	int pinType;
	ExternParamsList initialiserList;

	CVariableDeclaration(CInteger& arraySize,bool internal,CIdentifier& id, CInteger& size) :
		internal(internal), id(id), arraySize(arraySize),size(size),pinType(0) { }
	CVariableDeclaration(CInteger& arraySize,bool internal,CIdentifier& id, CInteger& size,AliasList& aliases) :
		internal(internal), id(id), arraySize(arraySize),size(size),aliases(aliases),pinType(0) { }
	CVariableDeclaration(CIdentifier& id, CInteger& size,int pinType) :
		id(id), arraySize(notArray),size(size),pinType(pinType) { }
	CVariableDeclaration(CIdentifier& id, CInteger& size,AliasList& aliases,int pinType) :
		id(id), arraySize(notArray),size(size),aliases(aliases),pinType(pinType) { }

	void AddInitialisers(ExternParamsList &initialisers) {initialiserList=initialisers;}

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);

	void CreateWriteAccessor(CodeGenContext& context,BitVariable& var,const std::string& moduleName, const std::string& name);
	void CreateReadAccessor(CodeGenContext& context,BitVariable& var);
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
};

class CStateDeclaration : public CStatement {
public:
	CIdentifier& id;
	bool autoIncrement;
	llvm::BasicBlock* entry;
	llvm::BasicBlock* exit;
	CStatesDeclaration* child;
	CStateDeclaration(CIdentifier& id) :
		id(id),autoIncrement(false),entry(NULL),exit(NULL),child(NULL) { }
	virtual llvm::Value* codeGen(CodeGenContext& context,llvm::Function* parent);
};

class CStatesDeclaration : public CStatement {
public:
	StateList states;
	std::string label;
	StatesList children;
	llvm::BasicBlock* exitState;
	llvm::Value* optocurState;
	CBlock& block;
	CStatesDeclaration(StateList& states,CBlock& block) :
		states(states),block(block) { }

	CStateDeclaration* getStateDeclaration(const CIdentifier& id) { for (int a=0;a<states.size();a++) { if (states[a]->id.name == id.name) return states[a]; } return NULL; }
	int getStateDeclarationIndex(const CIdentifier& id) { for (int a=0;a<states.size();a++) { if (states[a]->id.name == id.name) return a; } return -1; }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateTest : public CExpression {
public:
	StateIdentList stateIdents;
	CStateTest(StateIdentList& stateIdents) :
		stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateJump : public CStatement {
public:
	StateIdentList stateIdents;
	CStateJump(StateIdentList& stateIdents) :
		stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStatePush : public CStatement {
public:
	StateIdentList stateIdents;
	CStatePush(StateIdentList& stateIdents) :
		stateIdents(stateIdents) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStatePop : public CStatement {
public:
	CIdentifier& ident;
	CStatePop(CIdentifier& ident) :
		ident(ident) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};


class CIfStatement : public CStatement
{
public:
	CExpression& expr;
	CBlock& block;
	CIfStatement(CExpression& expr, CBlock& block) :
		expr(expr),block(block) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CStateDefinition : public CStatement {
public:
	const CIdentifier& id;
	CBlock& block;
	CStateDefinition(const CIdentifier& id, CBlock& block) :
		id(id), block(block) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CTrigger : public CExpression {
public:
	int type;
	CInteger& param1;
	CInteger& param2;
	static CInteger zero;
	CTrigger(int type) :
		type(type), param1(zero),param2(zero) { }
	CTrigger(int type, CInteger& param1, CInteger& param2) :
		type(type), param1(param1), param2(param2) { }
	virtual llvm::Value* codeGen(CodeGenContext& context,BitVariable& pin,llvm::Value* function);
};


class CHandlerDeclaration : public CStatement {
public:
	const CIdentifier& id;
	CTrigger& trigger;
	CBlock& block;
	llvm::GlobalVariable* depthTree;
	llvm::GlobalVariable* depthTreeIdx;
	CStatesDeclaration* child;
	CHandlerDeclaration(const CIdentifier& id, CTrigger& trigger,CBlock& block) :
		id(id), trigger(trigger),block(block) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CMappingDeclaration : public CStatement {
public:
	MappingList mappings;
	const CIdentifier& ident;
	CInteger& size;

	CMappingDeclaration(const CIdentifier& ident, CInteger& size, MappingList& mappings) :
		mappings(mappings), ident(ident), size(size) { }
	
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CInstruction : public CStatement {
public:
	CIdentifier& table;
	CString& mnemonic;
	OperandList operands;
	CBlock& block;
	static CIdentifier emptyTable;

	CInstruction(CIdentifier& table,CString& mnemonic,OperandList& operands, CBlock& block) :
		table(table),mnemonic(mnemonic),operands(operands), block(block) { }
	CInstruction(CString& mnemonic,OperandList& operands, CBlock& block) :
		table(emptyTable),mnemonic(mnemonic),operands(operands), block(block) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);

	CString& processMnemonic(CodeGenContext& context,CString& in,llvm::APInt& opcode,unsigned num);
};

class CExecute : public CStatement {
public:
	CIdentifier& table;
	CIdentifier& opcode;
	static CIdentifier emptyTable;
	CExecute(CIdentifier& table,CIdentifier& opcode) :
		table(table),opcode(opcode) { }
	CExecute(CIdentifier& opcode) :
		table(emptyTable),opcode(opcode) { }
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CAffect : public CExpression {
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

	CAffect(CIdentifier& ident,int type) :
		ident(ident),type(type),param(emptyParam),ov1(ident),ov2(ident) { }
	CAffect(CIdentifier& ident,int type,CInteger& param) :
		ident(ident),type(type),param(param),ov1(ident),ov2(ident) { }
	CAffect(CIdentifier& ident,int type,CExpression& ov1,CExpression& ov2,CInteger& param) :
		ident(ident),type(type),opType(type),param(param),ov1(ov1),ov2(ov2) { }

	llvm::Value* codeGenCarry(CodeGenContext& context,llvm::Value* exprResult,llvm::Value* lhs,llvm::Value* rhs,int type);
	llvm::Value* codeGenFinal(CodeGenContext& context,llvm::Value* exprResult);
};

class CAffector : public CExpression {
public:
	AffectorList affectors;
	CExpression& expr;
	
	CAffector(AffectorList& affectors,CExpression& expr) :
		affectors(affectors),expr(expr) { }

	virtual void prePass(CodeGenContext& context);

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CExternDecl : public CStatement {
public:
	ExternParamsList returns;
	CIdentifier& name;
	ExternParamsList params;

	CExternDecl(ExternParamsList& returns,CIdentifier& name,ExternParamsList& params) :
		returns(returns), name(name), params(params) { }
	CExternDecl(CIdentifier& name,ExternParamsList& params) :
		name(name), params(params) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CFuncCall : public CExpression {
public:
	CIdentifier& name;
	ParamsList& params;

	CFuncCall(CIdentifier& name,ParamsList& params) :
		name(name), params(params) { }
	
	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CFunctionDecl : public CStatement {
public:
	bool internal;
	NamedParamsList returns;
	CIdentifier& name;
	NamedParamsList params;
	CBlock& block;

	CFunctionDecl(bool internal,NamedParamsList& returns,CIdentifier& name,NamedParamsList& params,CBlock &block) :
		internal(internal), returns(returns), name(name), params(params),block(block) { }
	CFunctionDecl(bool internal,CIdentifier& name,NamedParamsList& params,CBlock &block) :
		internal(internal), name(name), params(params),block(block) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CParamDecl : public CStatement {
public:
	CIdentifier& id;
	CInteger& size;
	CParamDecl(CIdentifier& id, CInteger& size) :
		id(id), size(size) { }
};

class CInstance : public CStatement {
public:
	CString&	filename;
	CIdentifier&	ident;

	CInstance(CString& filename,CIdentifier& ident) :
		filename(filename), ident(ident) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CExchange : public CStatement {
public:
	CIdentifier&	lhs;
	CIdentifier&	rhs;

	CExchange(CIdentifier& lhs,CIdentifier& rhs) :
		lhs(lhs), rhs(rhs) { }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

