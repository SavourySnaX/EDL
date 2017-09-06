#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away
extern void PrintErrorWholeLine(const YYLTYPE &location, const char *errorstring, ...);			// Todo refactor away

bool CBinaryOperator::IsCarryExpression() const
{
	bool allCarry=true;

	if (!lhs.IsLeaf())
	{
		allCarry &= lhs.IsCarryExpression();
	}
	if (!rhs.IsLeaf())
	{
		allCarry &= rhs.IsCarryExpression();
	}

	return (op==TOK_ADD || op==TOK_DADD || op==TOK_DSUB || op==TOK_SUB) && allCarry;
}

void CBinaryOperator::prePass(CodeGenContext& context)
{
	lhs.prePass(context);
	rhs.prePass(context);
}

llvm::Value* CBinaryOperator::codeGen(CodeGenContext& context)
{
	llvm::Value* left;
	llvm::Value* right;

	left = lhs.codeGen(context);
	right = rhs.codeGen(context);

	return codeGen(context,left,right);
}

llvm::Value* CBinaryOperator::codeGen(CodeGenContext& context,llvm::Value* left,llvm::Value* right)
{
	if (left && right && left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
	{
		llvm::IntegerType* leftType = llvm::cast<llvm::IntegerType>(left->getType());
		llvm::IntegerType* rightType = llvm::cast<llvm::IntegerType>(right->getType());

		bool signExtend=false;
		switch (op)
		{
		default:
			break;
		case TOK_DADD:
		case TOK_DSUB:
			signExtend=true;
			break;
		}

		// perform left or right hand side promotion to appropriate size
		if (leftType->getBitWidth() < rightType->getBitWidth())
		{
			llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(left,signExtend,rightType,false);

			left = llvm::CastInst::Create(op,left,rightType,"cast",context.currentBlock());
		}
		if (leftType->getBitWidth() > rightType->getBitWidth())
		{
			llvm::Instruction::CastOps op = llvm::CastInst::getCastOpcode(right,signExtend,leftType,false);

			right = llvm::CastInst::Create(op,right,leftType,"cast",context.currentBlock());
		}

		switch (op) 
		{
		default:
			assert(0 && "Internal Error - Unimplemented binary operator!");
			context.errorFlagged=true;
			return nullptr;
		case TOK_ADD:
		case TOK_DADD:
		case TOK_SUB:
		case TOK_DSUB:
			{
				llvm::Value* result;
				if (op==TOK_ADD || op==TOK_DADD)
				{
					result=llvm::BinaryOperator::Create(llvm::Instruction::Add,left,right,"",context.currentBlock());
				}
				else
				{
					result=llvm::BinaryOperator::Create(llvm::Instruction::Sub,left,right,"",context.currentBlock());
				}
				
				// If there are affectors, perform checks for overflow and carry generation (since these are only defined for ADD/SUB operations)
				if (context.curAffectors.size())
				{
					bool hasMixedOperations = false;
					for (int a = 0; a < context.curAffectors.size(); a++)
					{
						switch (context.curAffectors[a]->type)
						{
						case TOK_NOOVERFLOW:
						case TOK_OVERFLOW:
							if (context.curAffectors[a]->opType == TOK_NOOVERFLOW || context.curAffectors[a]->opType == TOK_OVERFLOW)
							{
								context.curAffectors[a]->opType = op;
							}
							else
							{
								switch (context.curAffectors[a]->opType)
								{
								case TOK_ADD:
								case TOK_DADD:
									if (op != TOK_ADD && op != TOK_DADD)
									{
										hasMixedOperations = true;
									}
									break;
								case TOK_SUB:
								case TOK_DSUB:
									if (op != TOK_SUB && op != TOK_DSUB)
									{
										hasMixedOperations = true;
									}
									break;
								}
							}
							break;
						case TOK_NOCARRY:
						case TOK_CARRY:
							context.curAffectors[a]->codeGenCarry(context, result, left, right, op);
							break;
						}
					}

					if (hasMixedOperations)
					{
						PrintErrorFromLocation(operatorLoc,"OVERFLOW requires all operators to be either +(+) or -(-) only");
						context.errorFlagged = true;
						return nullptr;
					}
				}

				return result;
			}
		case TOK_MUL:
			return llvm::BinaryOperator::Create(llvm::Instruction::Mul,left,right,"",context.currentBlock());
		case TOK_DIV:
			return llvm::BinaryOperator::Create(llvm::Instruction::UDiv,left,right,"",context.currentBlock());
		case TOK_MOD:
			return llvm::BinaryOperator::Create(llvm::Instruction::URem,left,right,"",context.currentBlock());
		case TOK_SDIV:
			return llvm::BinaryOperator::Create(llvm::Instruction::SDiv,left,right,"",context.currentBlock());
		case TOK_SMOD:
			return llvm::BinaryOperator::Create(llvm::Instruction::SRem,left,right,"",context.currentBlock());
		case TOK_CMPEQ:
			return llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_EQ,left, right, "", context.currentBlock());
		case TOK_CMPNEQ:
			return llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_NE,left, right, "", context.currentBlock());
		case TOK_CMPLESS:
			return llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_ULT,left, right, "", context.currentBlock());
		case TOK_CMPLESSEQ:
			return llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_ULE,left, right, "", context.currentBlock());
		case TOK_CMPGREATER:
			return llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_UGT,left, right, "", context.currentBlock());
		case TOK_CMPGREATEREQ:
			return llvm::CmpInst::Create(llvm::Instruction::ICmp,llvm::ICmpInst::ICMP_UGE,left, right, "", context.currentBlock());
		case TOK_BAR:
			return llvm::BinaryOperator::Create(llvm::Instruction::Or,left,right,"",context.currentBlock());
		case TOK_AMP:
			return llvm::BinaryOperator::Create(llvm::Instruction::And,left,right,"",context.currentBlock());
		case TOK_HAT:
			return llvm::BinaryOperator::Create(llvm::Instruction::Xor,left,right,"",context.currentBlock());
		case TOK_TILDE:
			return llvm::BinaryOperator::Create(llvm::Instruction::Xor,left,llvm::ConstantInt::get(TheContext,~llvm::APInt(leftType->getBitWidth(),0)),"",context.currentBlock());
		}
	}

	PrintErrorWholeLine(operatorLoc, "Illegal types in expression");
	context.errorFlagged=true;
	return nullptr;
}
