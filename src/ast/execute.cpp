#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/IR/Instructions.h>

CIdentifier CExecute::emptyTable("");

void CExecute::prePass(CodeGenContext& context)
{
	ExecuteInformation temp;

	temp.executeLoc = executeLoc;
	temp.blockEndForExecute = nullptr;
	temp.execute = this;
	temp.switchForExecute = nullptr;

	context.executeLocations[table.name].push_back(temp);
	tableOffset = context.executeLocations[table.name].size() - 1;
}

llvm::Value* CExecute::codeGen(CodeGenContext& context)
{
	llvm::Value* load = opcode.codeGen(context);

	if (load->getType()->isIntegerTy())
	{
		const llvm::IntegerType* typeForExecute = llvm::cast<llvm::IntegerType>(load->getType());

		ExecuteInformation& temp = context.executeLocations[table.name][tableOffset];

		temp.blockEndForExecute = context.makeBasicBlock("execReturn", context.currentBlock()->getParent());
		if (context.gContext.opts.traceUnimplemented)
		{
			llvm::BasicBlock* tempBlock = context.makeBasicBlock("default", context.currentBlock()->getParent());

			std::vector<llvm::Value*> args;

			// Handle variable promotion
			llvm::Type* ty = context.getIntType(32);
			llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(load, false, ty, false);

			llvm::Instruction* truncExt = llvm::CastInst::Create(op, load, ty, "cast", tempBlock);

			args.push_back(truncExt);
			llvm::CallInst *call = llvm::CallInst::Create(context.gContext.debugTraceMissing, args, "DEBUGTRACEMISSING", tempBlock);

			llvm::BranchInst::Create(temp.blockEndForExecute, tempBlock);

			temp.switchForExecute = llvm::SwitchInst::Create(load, tempBlock, 2 << typeForExecute->getBitWidth(), context.currentBlock());
		}
		else
		{
			temp.switchForExecute = llvm::SwitchInst::Create(load, temp.blockEndForExecute, 2 << typeForExecute->getBitWidth(), context.currentBlock());
		}

		context.setBlock(temp.blockEndForExecute);

        while (!temp.processLater.empty())
        {
            ExecuteSaved t = temp.processLater.top();
            LinkToSwitch(context, t.func, t.args, t.name, t.caseNum);
            temp.processLater.pop();
        }
	}
	return nullptr;
}
    
void CExecute::LinkToSwitch(CodeGenContext& context, llvm::Function* func, std::vector<llvm::Value*> args,std::string name, llvm::APInt& caseNum)
{
	ExecuteInformation& temp = context.executeLocations[table.name][tableOffset];

    if (temp.blockEndForExecute)
    {
		llvm::Function *parentFunction = temp.blockEndForExecute->getParent();
		context.gContext.scopingStack.push(parentFunction->getSubprogram());

		llvm::BasicBlock* tempBlock = context.makeBasicBlock("callOut" + table.name + name, temp.blockEndForExecute->getParent());
		llvm::CallInst* fcall = llvm::CallInst::Create(func, args, "", tempBlock);
		if (context.gContext.opts.generateDebug)
		{
			fcall->setDebugLoc(llvm::DebugLoc::get(temp.executeLoc.first_line, temp.executeLoc.first_column, context.gContext.scopingStack.top()));
		}
		llvm::BranchInst::Create(temp.blockEndForExecute, tempBlock);
		temp.switchForExecute->addCase(context.getConstantInt(caseNum), tempBlock);

		context.gContext.scopingStack.pop();
	}
    else
    {
        // We haven't yet initialised this EXECUTE statement (instruction appears before EXECUTE in source)
        ExecuteSaved t;
        t.func = func;
        t.args = args;
        t.name = name;
        t.caseNum = caseNum;
        temp.processLater.push(t);
    }
}
