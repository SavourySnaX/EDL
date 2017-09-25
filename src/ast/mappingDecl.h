#pragma once

class CMappingDeclaration : public CStatement
{
private:
	llvm::Function* getByMapping;
	llvm::Function* setByMapping;
	llvm::Function* setByMappingCast;
	llvm::Function* doByMapping;

	llvm::Function* GenerateGetByMappingFunction(CodeGenContext& context,bool asVoid);
	llvm::Function* GenerateSetByMappingFunction(CodeGenContext& context,bool withCast);
public:
	MappingList mappings;
	const CIdentifier& ident;
	CInteger& size;
	YYLTYPE blockStart;
	YYLTYPE blockEnd;

	CMappingDeclaration(const CIdentifier& ident, CInteger& size, MappingList& mappings,YYLTYPE* blockStart,YYLTYPE* blockEnd) : mappings(mappings), ident(ident), size(size), blockStart(*blockStart), blockEnd(*blockEnd) 
	{ 
		getByMapping = nullptr;
		setByMapping = nullptr;
		doByMapping = nullptr;
	}

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);

	llvm::Value* generateCallGetByMapping(CodeGenContext& context,const YYLTYPE& modLoc,const YYLTYPE& nameLoc);
	llvm::Value* generateCallSetByMapping(CodeGenContext& context, const YYLTYPE& modLoc, const YYLTYPE& nameLoc, llvm::Value* rhs);
	llvm::Value* generateCallSetByMappingCast(CodeGenContext& context, const YYLTYPE& modLoc, const YYLTYPE& nameLoc, llvm::Value* rhs,CCastOperator* cast);
};
