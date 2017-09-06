#pragma once

class CDebugTrace : public CStatement
{
public:
	int currentBase;

	virtual bool isModifier() const { return false; }
};

class CDebugLine : public CStatement
{
public:
	DebugList debug;
	YYLTYPE debugLocation;

	CDebugLine(DebugList& debug, YYLTYPE* debugLocation) : debug(debug), debugLocation(*debugLocation) { }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceString : public CDebugTrace
{
public:
	CString& string;

	CDebugTraceString(CString& string) : string(string) { }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceInteger : public CDebugTrace {
public:
	CInteger& integer;

	CDebugTraceInteger(CInteger& integer) : integer(integer) { }

	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceIdentifier : public CDebugTrace
{
public:
	CIdentifier& ident;

	CDebugTraceIdentifier(CIdentifier& ident) : ident(ident) { }

	llvm::Value* generate(CodeGenContext& context, llvm::Value* loadedValue);
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class CDebugTraceBase : public CDebugTrace
{
public:
	CInteger& integer;

	CDebugTraceBase(CInteger& integer) : integer(integer) { }

	virtual bool isModifier() const { return true; }
};

