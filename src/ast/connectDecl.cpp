#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away
extern void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...);			// Todo refactor away

void CConnectDeclaration::prePass(CodeGenContext& context)
{

}

llvm::Value* CConnectDeclaration::codeGen(CodeGenContext& context)
{
	std::vector<llvm::Type*> FuncTy_8_args;
	llvm::FunctionType* FuncTy_8;
	llvm::Function* func = nullptr;

	// 1 argument at present, same as the ident - contains a single bit
	FuncTy_8_args.push_back(context.getIntType(1));

	FuncTy_8 = llvm::FunctionType::get(context.getVoidType(), FuncTy_8_args, false);

	func = context.makeFunction(FuncTy_8, llvm::GlobalValue::ExternalLinkage, context.moduleName + context.getSymbolPrefix() + ident.name);

	context.StartFunctionDebugInfo(func, statementLoc);

	context.m_externFunctions[ident.name] = func;
	context.gContext.connectFunctions[func] = func;

	llvm::BasicBlock *bblock = context.makeBasicBlock("entry", func);

	context.pushBlock(bblock, blockStartLoc);

	llvm::Function::arg_iterator args = func->arg_begin();
	while (args != func->arg_end())
	{
		BitVariable temp(llvm::APInt(1,1),0);

		temp.value = &*args;
		temp.value->setName(ident.name);
		context.locals()[ident.name] = temp;
		args++;
	}

	// Quickly spin through connection list and find the last bus tap
	size_t lastTap = 0;
	for (size_t searchLastTap = 0; searchLastTap < connects.size(); searchLastTap++)
	{
		if (connects[searchLastTap]->hasTap)
		{
			lastTap = searchLastTap;
		}
	}

	// This needs a re-write
	// for now - if a nullptr occurs in the input list, it is considered to be an isolation resistor,
	//
	// if only one side of the bus is driving, both sides get the final value as inputs
	// if neither side is driving, both sides get PULLUP
	// if both sides are driving, left and right sides get their respective values as input
	//
	//  A   |  B
	// Z Z  | Z Z		1
	// 1 1  | 1 1   
	//      |
	// a Z  | Z Z		2
	// A A  | A A
	//
	// Z Z  | b Z		3
	// B B  | B B
	//
	// a Z  | b Z		4
	// A A  | B B
	//
	// We can probably model this as -- Combine A Bus - seperately Combine B Bus - Generate an isDrivingBus for both sides - then select as :
	//
	// ~IsDrivingA & ~IsDrivingB - assign A inputs with A and B inputs with B  (both will be FF, but doesn't matter)
	// IsDrivingA & ~IsDrivingB - assign all inputs with A (simply set B=A)
	// ~IsDrivingA & IsDrivingB - assign all inputs with B (simply set A=B)
	// IsDrivingA & IsDrivingB - assign A inputs with A and B inputs with B
	//
	// So simplfication should be											1		2		3		4
	// outputA=Generate Bus A output										 FF		.a		 FF		.a
	// outputB=Generate Bus B output										 FF		 FF		.b		.b
	// IF isDrivingA ^ IsDrivingB											false	true	true	false
	//		outputB = select isDrivingA outputA outputB								a		b
	//		outputA = select isDrivingB outputB outputA								a		b
	// assign A inputs with outputA
	// assign B inputs with outputB
	//
	//

	for (size_t a = 0; a < connects.size(); a++)
	{
		// First step, figure out number of buses

		int busCount = 1;	// will always be at least 1 bus, but ISOLATION can generate split buses
		for (size_t b = 0; b < connects[a]->connects.size(); b++)
		{
			if ((*connects[a]).connects[b] == nullptr)
			{
				busCount++;
			}
		}

		// Do a quick pass to build a list of Inputs and Outputs - Maintaining declaration order (perform some validation as we go)
		int *outCnt = new int[busCount];
		int *inCnt = new int[busCount];
		int *biCnt = new int[busCount];
		memset(outCnt, 0, busCount * sizeof(int));
		memset(inCnt, 0, busCount * sizeof(int));
		memset(biCnt, 0, busCount * sizeof(int));
		std::vector<CIdentifier*> *ins = new std::vector<CIdentifier*>[busCount];
		std::vector<CExpression*> *outs = new std::vector<CExpression*>[busCount];

		int curBus = 0;
		for (size_t b = 0; b < connects[a]->connects.size(); b++)
		{
			if ((*connects[a]).connects[b] == nullptr)
			{
				// bus split
				curBus++;
				continue;
			}
			if ((*connects[a]).connects[b]->IsIdentifierExpression())
			{
				CIdentifier* ident = (CIdentifier*)(*connects[a]).connects[b];

				BitVariable var;

				if (!context.LookupBitVariable(var, ident->module, ident->name, ident->modLoc, ident->nameLoc))
				{
					return nullptr;
				}
				else
				{
					switch (var.pinType)
					{
					case 0:
						//							std::cerr << "Variable : Acts as Output " << ident->name << std::endl;
						outs[curBus].push_back(ident);
						outCnt[curBus]++;
						break;
					case TOK_IN:
						//							std::cerr << "PIN : Acts as Input " << ident->name << std::endl;
						ins[curBus].push_back(ident);
						inCnt[curBus]++;
						break;
					case TOK_OUT:
						//							std::cerr << "PIN : Acts as Output " << ident->name << std::endl;
						outs[curBus].push_back(ident);
						outCnt[curBus]++;
						break;
					case TOK_BIDIRECTIONAL:
						//							std::cerr << "PIN : Acts as Input/Output " << ident->name << std::endl;
						ins[curBus].push_back(ident);
						outs[curBus].push_back(ident);
						biCnt[curBus]++;
						break;
					}
				}
			}
			else
			{
				//				std::cerr << "Complex Expression : Acts as Output " << std::endl;
				outs[curBus].push_back((*connects[a]).connects[b]);
				outCnt[curBus]++;
			}
		}

		for (size_t checkBus = 0; checkBus <= curBus; checkBus++)
		{
			if (outCnt[checkBus] == 0 && inCnt[checkBus] == 0 && biCnt[checkBus] == 0)
			{
				PrintErrorWholeLine(connects[a]->statementLoc, "No possible routing! - no connections");
				context.errorFlagged = true;
				return nullptr;
			}

			if (outCnt[checkBus] == 0 && inCnt[checkBus] > 0 && biCnt[checkBus] == 0)
			{
				PrintErrorWholeLine(connects[a]->statementLoc, "No possible routing! - all connections are input");
				context.errorFlagged = true;
				return nullptr;
			}

			if (outCnt[checkBus] > 0 && inCnt[checkBus] == 0 && biCnt[checkBus] == 0)
			{
				PrintErrorWholeLine(connects[a]->statementLoc, "No possible routing! - all connections are output");
				context.errorFlagged = true;
				return nullptr;
			}
		}
		//		std::cerr << "Total Connections : in " << inCnt << " , out " << outCnt << " , bidirect " << biCnt << std::endl;

		llvm::Value** busOutResult = new llvm::Value*[busCount];
		llvm::Value** busIsDrivingResult = new llvm::Value*[busCount];

		// Generate bus output signals (1 per bus, + HiZ indicator per bus)
		for (curBus = 0; curBus < busCount; curBus++)
		{
			busOutResult[curBus] = nullptr;
			busIsDrivingResult[curBus] = context.getConstantZero(1);	// 0 for not driving bus

			for (size_t o = 0; o < outs[curBus].size(); o++)
			{
				CIdentifier* ident;
				BitVariable var;

				if (!outs[curBus][o]->IsIdentifierExpression())
				{
					busIsDrivingResult[curBus] = context.getConstantOnes(1);	// complex expression, always a bus driver
				}
				else
				{
					ident = (CIdentifier*)outs[curBus][o];

					if (!context.LookupBitVariable(var, ident->module, ident->name, ident->modLoc, ident->nameLoc))
					{
						return nullptr;
					}

					if (var.impedance == nullptr)
					{
						busIsDrivingResult[curBus] = context.getConstantOnes(1);	// complex expression, always a bus driver
					}
					else
					{
						llvm::Value* fetchDriving = new llvm::LoadInst(var.impedance, "fetchImpedanceForIsolation", false, bblock);
						fetchDriving = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::ICmpInst::ICMP_EQ, fetchDriving, context.getConstantZero(var.size.getLimitedValue()), "isNotImpedance", bblock);

						llvm::Instruction* I = llvm::BinaryOperator::Create(llvm::Instruction::Or, fetchDriving, busIsDrivingResult[curBus], "isDriving", context.currentBlock());
						if (context.gContext.opts.generateDebug)
						{
							I->setDebugLoc(llvm::DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
						}
						busIsDrivingResult[curBus] = I;
					}
				}


				llvm::Value* tmp = outs[curBus][o]->codeGen(context);

				if (o == 0)
				{
					busOutResult[curBus] = tmp;
				}
				else
				{
					llvm::Type* lastType = busOutResult[curBus]->getType();
					llvm::Type* tmpType = tmp->getType();
					if (lastType != tmpType)
					{
						// Should be integer types at the point
						if (!lastType->isIntegerTy() || !tmpType->isIntegerTy())
						{
							PrintErrorWholeLine(connects[a]->statementLoc, "(TODO) Expected integer types");
							context.errorFlagged = true;
							return nullptr;
						}
						if (lastType->getPrimitiveSizeInBits() < tmpType->getPrimitiveSizeInBits())
						{
							// need cast last zeroextend
							busOutResult[curBus] = new llvm::ZExtInst(busOutResult[curBus], tmpType, "ZExt", context.currentBlock());
						}
						else
						{
							// need cast tmp zeroextend
							tmp = new llvm::ZExtInst(tmp, lastType, "ZExt", context.currentBlock());
						}
					}

					llvm::Instruction* I = llvm::BinaryOperator::Create(llvm::Instruction::And, tmp, busOutResult[curBus], "PullUpCombine", context.currentBlock());
					if (context.gContext.opts.generateDebug)
					{
						I->setDebugLoc(llvm::DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
					}
					busOutResult[curBus] = I;
				}
			}
		}

		// Output busses all generated - generate the code to fix up the buses as per the rules above - todo fix this for beyond 2 bus case

		if (busCount == 2)
		{
			llvm::Value* needsSwap = llvm::BinaryOperator::Create(llvm::Instruction::Xor, busIsDrivingResult[0], busIsDrivingResult[1], "RequiresSwap", context.currentBlock());
			llvm::Value* swapA = llvm::BinaryOperator::Create(llvm::Instruction::And, busIsDrivingResult[1], needsSwap, "setAtoB", context.currentBlock());
			llvm::Value* swapB = llvm::BinaryOperator::Create(llvm::Instruction::And, busIsDrivingResult[0], needsSwap, "setBtoA", context.currentBlock());

			{// extend type widths to match
				llvm::Type* lastType = busOutResult[0]->getType();
				llvm::Type* tmpType = busOutResult[1]->getType();
				if (lastType != tmpType)
				{
					// Should be integer types at the point
					if (!lastType->isIntegerTy() || !tmpType->isIntegerTy())
					{
						PrintErrorWholeLine(connects[a]->statementLoc, "(TODO) Expected integer types");
						context.errorFlagged = true;
						return nullptr;
					}
					if (lastType->getPrimitiveSizeInBits() < tmpType->getPrimitiveSizeInBits())
					{
						// need cast last zeroextend
						busOutResult[0] = new llvm::ZExtInst(busOutResult[0], tmpType, "ZExt", context.currentBlock());
					}
					else
					{
						// need cast tmp zeroextend
						busOutResult[1] = new llvm::ZExtInst(busOutResult[1], lastType, "ZExt", context.currentBlock());
					}
				}
			}


			llvm::Instruction* swapAB = llvm::SelectInst::Create(swapA, busOutResult[1], busOutResult[0], "SwapAwithB", context.currentBlock());
			llvm::Instruction* swapBA = llvm::SelectInst::Create(swapB, busOutResult[0], busOutResult[1], "SwapBwithA", context.currentBlock());
			if (context.gContext.opts.generateDebug)
			{
				swapAB->setDebugLoc(llvm::DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
				swapBA->setDebugLoc(llvm::DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
			}

			busOutResult[0] = swapAB;
			busOutResult[1] = swapBA;
		}
		if (busCount > 2)
		{
			PrintErrorWholeLine(connects[a]->statementLoc, "(TODO) Bus arbitration for >2 buses not implemented yet");
			context.errorFlagged = true;
			return nullptr;
		}

		for (curBus = 0; curBus < busCount; curBus++)
		{
			for (size_t i = 0; i < ins[curBus].size(); i++)
			{
				BitVariable var;

				if (!context.LookupBitVariable(var, ins[curBus][i]->module, ins[curBus][i]->name, ins[curBus][i]->modLoc, ins[curBus][i]->nameLoc))
				{
					return nullptr;
				}

				llvm::Instruction* I = CAssignment::generateAssignment(var, *ins[curBus][i], busOutResult[curBus], context);
				if (context.gContext.opts.generateDebug)
				{
					I->setDebugLoc(llvm::DebugLoc::get((*connects[a]).statementLoc.first_line, (*connects[a]).statementLoc.first_column, context.gContext.scopingStack.top()));
				}

				if (curBus == 0 && i == 0 && connects[a]->hasTap && context.gContext.opts.optimisationLevel < 3)		// only tap the first bus for now
				{
					// Generate a call to AddPinRecord(cnt,val,name,last)

					std::vector<llvm::Value*> args;

					// Create global string for bus tap name
					llvm::Value* nameRef = connects[a]->tapName.codeGen(context);

					// Handle decode width promotion
					llvm::Value* DW = connects[a]->decodeWidth.codeGen(context);
					llvm::Type* tyDW = context.getIntType(32);
					llvm::Instruction::CastOps opDW = llvm::CastInst::getCastOpcode(DW, false, tyDW, false);

					llvm::Instruction* truncExtDW = llvm::CastInst::Create(opDW, DW, tyDW, "cast", context.currentBlock());


					// Handle variable promotion
					llvm::Type* ty = context.getIntType(32);
					llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(busOutResult[curBus], false, ty, false);

					llvm::Instruction* truncExt = llvm::CastInst::Create(op, busOutResult[curBus], ty, "cast", context.currentBlock());
					args.push_back(context.getConstantInt(llvm::APInt(8, var.trueSize.getLimitedValue(), false)));
					args.push_back(truncExt);
					args.push_back(nameRef);
					args.push_back(truncExtDW);
					args.push_back(context.getConstantInt(llvm::APInt(8, a == lastTap ? 1 : 0, false)));
					llvm::CallInst *call = llvm::CallInst::Create(context.debugBusTap, args, "", context.currentBlock());
				}
			}
		}
	}

	llvm::Instruction* I = context.makeReturn(context.currentBlock());
	if (context.gContext.opts.generateDebug)
	{
		I->setDebugLoc(llvm::DebugLoc::get(blockEndLoc.first_line, blockEndLoc.first_column, context.gContext.scopingStack.top()));
	}

	context.popBlock(blockEndLoc);

	context.EndFunctionDebugInfo();

	return func;
}
