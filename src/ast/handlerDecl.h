#pragma once

class CHandlerDeclaration : public CStatement
{
public:
	const CIdentifier& id;
	CTrigger& trigger;
	CBlock& block;
	llvm::GlobalVariable* depthTree;
	llvm::GlobalVariable* depthTreeIdx;
	CStatesDeclaration* child;
	YYLTYPE handlerLoc;

	CHandlerDeclaration(const CIdentifier& id, CTrigger& trigger, CBlock& block, YYLTYPE *handlerLoc) : id(id), trigger(trigger), block(block), handlerLoc(*handlerLoc) { }

	virtual void prePass(CodeGenContext& context);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};
