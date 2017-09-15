#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Value.h>

void CStateDefinition::prePass(CodeGenContext& context)
{
	CStatesDeclaration* pStates = context.currentState();
	if (pStates)
	{
		CStateDeclaration* pState = pStates->getStateDeclaration(id);

		if (pState)
		{
			context.pushIdent(&id);
			block.prePass(context);
			context.popIdent();
		}
	}
}

llvm::Value* CStateDefinition::codeGen(CodeGenContext& context)
{
	CStatesDeclaration* pStates = context.currentState();
	if (pStates)
	{
		CStateDeclaration* pState = pStates->getStateDeclaration(id);

		if (pState)
		{
			context.pushBlock(pState->entry, block.blockStartLoc);
			block.codeGen(context);
			pState->entry = context.currentBlock();
			context.popBlock(block.blockEndLoc);
			return nullptr;
		}
		else
		{
			return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, id.nameLoc, "Attempt to define unknown state");
		}
	}
	else
	{
		return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, id.nameLoc, "Attempt to define state entry when no states on stack");
	}
}
