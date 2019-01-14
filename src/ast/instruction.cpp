#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

CIdentifier CInstruction::emptyTable("");

std::string CInstruction::EscapeString(const std::string &in) const
{
	std::string ret=in;
	std::replace_if(ret.begin(), ret.end(), [in](const char& c) -> bool 
		{const char okChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"; return strchr(okChars, c) == nullptr; }, '_');

	return ret;
}

CString& CInstruction::processMnemonic(CodeGenContext& context, CString& in, llvm::APInt& opcode, unsigned num) const
{
	static CString temp("");

	temp.quoted = "";

	const char* ptr = in.quoted.c_str();

	int state = 0;
	while (*ptr)
	{
		switch (state)
		{
		case 0:
			if (*ptr == '%')
			{
				state = 1;
			}
			else
			{
				temp.quoted += *ptr;
			}
			break;
		case 1:
			if (*ptr == 'M')
			{
				state = 2;
				break;
			}
			if (*ptr == '$')
			{
				state = 3;
				break;
			}

			state = 0;
			break;
		case 2:
		{
			unsigned offset = *ptr - '0';
			const CString* res = operands[0]->GetString(context, num, offset);
			if (res)
			{
				temp.quoted += res->quoted.substr(1, res->quoted.length() - 2);
			}
			state = 0;
		}
		break;
		case 3:
			// for now simply output the reference as %ref (debug harness will have to deal with these references (which should be pc + refnum)
		{
			temp.quoted += '%';
			temp.quoted += *ptr;
			state = 0;
		}
		break;
		}
		ptr++;
	}

	return temp;
}

void CInstruction::prePass(CodeGenContext& context)
{
	block.prePass(context);
}

llvm::Value* CInstruction::codeGen(CodeGenContext& context)
{
	std::vector<llvm::Type*> argTypes;
	operands[0]->GetArgTypes(context,argTypes);

	// First up, get the first operand (this must be a computable constant!) - remaining operands are only used for asm/disasm generation
	unsigned numOpcodes = operands[0]->GetNumComputableConstants(context);
	if (numOpcodes == 0)
	{
		return context.gContext.ReportError(nullptr, EC_ErrorWholeLine, statementLoc, "Opcode for instruction must be able to generate constants");
	}

	llvm::Function* function = nullptr;
	for (unsigned a = 0; a < numOpcodes; a++)
	{
		llvm::APInt opcode = operands[0]->GetComputableConstant(context, a);

		CString disassembled = processMnemonic(context, mnemonic, opcode, a);
		CDebugTraceString opcodeString(disassembled);

		context.disassemblyTable[table.name][opcode] = disassembled.quoted;

		if (a == 0)
		{
			llvm::FunctionType *ftype = llvm::FunctionType::get(context.getVoidType(), argTypes, false);
			function = context.makeFunction(ftype, llvm::GlobalValue::PrivateLinkage, EscapeString(context.getSymbolPrefix() + "OPCODE_" + opcodeString.string.quoted.substr(1, opcodeString.string.quoted.length() - 2) + "_" + table.name + opcode.toString(16, false)));

			context.StartFunctionDebugInfo(function, statementLoc);

			llvm::BasicBlock *bblock = context.makeBasicBlock("entry", function);

			context.pushBlock(bblock, block.blockStartLoc);

			operands[0]->SetupFunctionArgs(context, function);
			operands[0]->DeclareLocal(context, a);

			block.codeGen(context);

			context.makeReturn(context.currentBlock());

			context.popBlock(block.blockEndLoc);

			context.EndFunctionDebugInfo();
		}
		if (context.isErrorFlagged())
		{
			return nullptr;
		}

		// Glue callee back into execute (locations are now fetched during prepass)
		for (int b = 0; b < context.executeLocations[table.name].size(); b++)
		{
			// Get reference to execute block and call CExecute::LinkToSwitch
			CExecute* execute = context.executeLocations[table.name][b].execute;

			std::vector<llvm::Value*> args;
			operands[0]->GetArgs(context, args, a);
			execute->LinkToSwitch(context, function, args, opcode.toString(16, false),opcode);
		}

	}
	return nullptr;
}
