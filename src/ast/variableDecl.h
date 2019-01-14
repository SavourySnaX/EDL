#pragma once

class CVariableDeclaration : public CStatement 
{
private:
	llvm::Instruction* writeAccessor;
public:
	static CInteger notArray;
	bool internal;
	CIdentifier& id;
	CInteger& arraySize;
	CInteger& size;
	MultiAliasList aliases;
	int pinType;
	ExternParamsList initialiserList;
	YYLTYPE declarationLoc;

	CVariableDeclaration(CInteger& arraySize, bool internal, CIdentifier& id, CInteger& size, YYLTYPE declarationLoc) :
		internal(internal), id(id), arraySize(arraySize), size(size), pinType(0), declarationLoc(declarationLoc) { }
	CVariableDeclaration(CInteger& arraySize, bool internal, CIdentifier& id, CInteger& size, MultiAliasList aliases, YYLTYPE declarationLoc) :
		internal(internal), id(id), arraySize(arraySize), size(size), aliases(aliases), pinType(0), declarationLoc(declarationLoc) { }
	CVariableDeclaration(CIdentifier& id, CInteger& size, int pinType, YYLTYPE declarationLoc) :
		id(id), arraySize(notArray), size(size), pinType(pinType), declarationLoc(declarationLoc) { }
	CVariableDeclaration(CIdentifier& id, CInteger& size, MultiAliasList aliases, int pinType, YYLTYPE declarationLoc) :
		id(id), arraySize(notArray), size(size), aliases(aliases), pinType(pinType), declarationLoc(declarationLoc) { }

	void AddInitialisers(ExternParamsList &initialisers) { initialiserList = initialisers; }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);

	void CreateWriteAccessor(CodeGenContext& context, BitVariable& var, const std::string& moduleName, const std::string& name, bool impedance);
	void CreateReadAccessor(CodeGenContext& context, BitVariable& var, bool impedance);
};
