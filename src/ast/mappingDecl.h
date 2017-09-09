#pragma once

class CMappingDeclaration : public CStatement
{
public:
	MappingList mappings;
	const CIdentifier& ident;
	CInteger& size;

	CMappingDeclaration(const CIdentifier& ident, CInteger& size, MappingList& mappings) : mappings(mappings), ident(ident), size(size) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
