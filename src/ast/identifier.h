#pragma once

class CIdentifier : public CBaseIdentifier 
{
public:
	std::string module;
	std::string name;
	YYLTYPE nameLoc;
	YYLTYPE modLoc;
	CIdentifier(const std::string& name) : name(name) { }
	CIdentifier(const std::string& name, YYLTYPE *nameLoc) : name(name), nameLoc(*nameLoc) { }
	CIdentifier(const std::string& module, const std::string& name, YYLTYPE *modLoc, YYLTYPE *nameLoc) : module(module + "."), name(name), nameLoc(*nameLoc), modLoc(*modLoc) { }
	static llvm::Value* trueSize(llvm::Value*, CodeGenContext& context, BitVariable& var);
	static llvm::Value* GetAliasedData(CodeGenContext& context, llvm::Value* in, BitVariable& var);
	virtual llvm::Value* codeGen(CodeGenContext& context);
	virtual bool IsIdentifierExpression() { return true; }
	virtual CExpression *GetExpression() const { return nullptr; }
};
