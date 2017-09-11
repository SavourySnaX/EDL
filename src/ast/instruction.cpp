#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...);			// Todo refactor away

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

	// First up, get the first operand (this must be a computable constant!) - remaining operands are only used for asm/disasm generation
	unsigned numOpcodes = operands[0]->GetNumComputableConstants(context);
	if (numOpcodes == 0)
	{
		PrintErrorWholeLine(statementLoc, "Opcode for instruction must be able to generate constants");
		context.errorFlagged = true;
		return nullptr;
	}

	for (unsigned a = 0; a < numOpcodes; a++)
	{
		llvm::APInt opcode = operands[0]->GetComputableConstant(context, a);

		CString disassembled = processMnemonic(context, mnemonic, opcode, a);
		CDebugTraceString opcodeString(disassembled);

		context.disassemblyTable[table.name][opcode] = disassembled.quoted;

		llvm::FunctionType *ftype = llvm::FunctionType::get(context.getVoidType(), argTypes, false);
		llvm::Function* function = llvm::Function::Create(ftype, llvm::GlobalValue::PrivateLinkage, EscapeString(context.symbolPrepend + "OPCODE_" + opcodeString.string.quoted.substr(1, opcodeString.string.quoted.length() - 2) + "_" + table.name + opcode.toString(16, false)), context.module);
		function->setDoesNotThrow();

		context.StartFunctionDebugInfo(function, statementLoc);

		llvm::BasicBlock *bblock = context.makeBasicBlock("entry", function);

		context.pushBlock(bblock, block.blockStartLoc);

		operands[0]->DeclareLocal(context, a);

		block.codeGen(context);

		context.makeReturn(context.currentBlock());

		context.popBlock(block.blockEndLoc);

		context.EndFunctionDebugInfo();

		if (context.errorFlagged)
		{
			return nullptr;
		}

		// Glue callee back into execute (assumes execute comes before instructions at all times for now)
		for (int b = 0; b < context.executeLocations[table.name].size(); b++)
		{
			llvm::Function *parentFunction = context.executeLocations[table.name][b].blockEndForExecute->getParent();
			context.gContext.scopingStack.push(parentFunction->getSubprogram());

			llvm::BasicBlock* tempBlock = context.makeBasicBlock("callOut" + table.name + opcode.toString(16, false), context.executeLocations[table.name][b].blockEndForExecute->getParent());
			std::vector<llvm::Value*> args;
			llvm::CallInst* fcall = llvm::CallInst::Create(function, args, "", tempBlock);
			if (context.gContext.opts.generateDebug)
			{
				fcall->setDebugLoc(llvm::DebugLoc::get(context.executeLocations[table.name][b].executeLoc.first_line, context.executeLocations[table.name][b].executeLoc.first_column, context.gContext.scopingStack.top()));
			}
			llvm::BranchInst::Create(context.executeLocations[table.name][b].blockEndForExecute, tempBlock);
			context.executeLocations[table.name][b].switchForExecute->addCase(context.getConstantInt(opcode), tempBlock);

			context.gContext.scopingStack.pop();
		}

	}
	return nullptr;
}
