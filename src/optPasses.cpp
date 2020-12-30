#include <map>
#include <string>

#include "optPasses.h"
#include "yyltype.h"
#include "ast.h"
#include "parser.hpp"
#include "bitvariable.h"

#include "generator.h"

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

#define DEBUG_STATE_SQUASH	0

char StateReferenceSquasher::ID = 0;
static llvm::RegisterPass<StateReferenceSquasher> X("StateReferenceSquasher", "Squashes multiple accesses to the same state, which should allow if/else behaviour", false, false);
char InOutReferenceSquasher::ID = 1;
static llvm::RegisterPass<InOutReferenceSquasher> XX("InOutReferenceSquasher", "Squashes multiple accesses to the same in/out pin, which should allow if/else behaviour", false, false);

bool StateReferenceSquasher::DoWork(llvm::Function &F, CodeGenContext* context)
{
	bool doneSomeWork = false;
	for (const auto& stateVar : context->states())
	{
#if DEBUG_STATE_SQUASH
		std::cerr << "-" << F.getName().str() << " " << stateVar.first << std::endl;
#endif
		// Search the instructions in the function, and find Load instructions
		llvm::Value* foundReference = nullptr;
		for (auto& instruction : llvm::instructions(F))
		{
			llvm::LoadInst* iRef = llvm::dyn_cast<llvm::LoadInst>(&instruction);
			llvm::StoreInst* sRef = llvm::dyn_cast<llvm::StoreInst>(&instruction);
			// We have a load instruction, we need to see if its operand points at our global state variable
			if (iRef && iRef->getOperand(0) == stateVar.second.currentState)
			{
				if (foundReference)
				{
#if DEBUG_STATE_SQUASH
					std::cerr << "--" << F.getName().str() << " " << iRef->getName().str() << std::endl;
#endif
					// Replace all uses of this instruction with original result,
					// I don't remove the current instruction - its upto a later pass to remove it
					iRef->replaceAllUsesWith(foundReference);//BinaryOperator::Create (Instruction::Or,foundReference,foundReference,"",iRef));
					doneSomeWork = true;
				}
				else
				{
#if DEBUG_STATE_SQUASH
					std::cerr << "==" << F.getName().str() << " " << iRef->getName().str() << std::endl;
#endif
					foundReference = iRef;
				}
			}
			// We have a store instruction, we need to see if its operand points at our global state variable
			if (sRef && sRef->getOperand(1) == stateVar.second.currentState)
			{
				foundReference = sRef->getOperand(0);
			}
		}

	}
	return doneSomeWork;
}

bool StateReferenceSquasher::runOnFunction(llvm::Function &F)
{
	bool doneSomeWork = false;

#if DEBUG_STATE_SQUASH
	std::cerr << "Running stateReferenceSquasher" << std::endl;
#endif
	doneSomeWork |= DoWork(F, ourContext);
	for (const auto& include : ourContext->m_includes)
	{
#if DEBUG_STATE_SQUASH
		std::cerr << F.getName().str() << " " << include.second->states().size() << std::endl;
#endif
		doneSomeWork |= DoWork(F, include.second);
	}

	return doneSomeWork;
}

bool InOutReferenceSquasher::DoWork(llvm::Function &F, CodeGenContext* context)
{
	llvm::BasicBlock& hoistHere = F.getEntryBlock();
	llvm::Instruction* hoistBefore = hoistHere.getTerminator();

	// InOut squasher re-orders the inputs, this is not a safe operation for CONNECT lists, so we exclude them here
	if (context->gContext.connectFunctions.find(&F) != context->gContext.connectFunctions.end())
	{
		return false;
	}

	bool doneSomeWork = false;
	for (const auto& bitVar : context->globals())
	{
		if (bitVar.second.pinType == TOK_IN)
		{
#if DEBUG_STATE_SQUASH
			std::cerr << "-" << F.getName().str() << " " << bitVar.first << std::endl;
#endif
			// Search the instructions in the function, and find Load instructions
			llvm::Value* foundReference = nullptr;

			// Find and hoist all Pin loads to entry (avoids need to phi - hopefully llvm later passes will clean up)
			bool hoisted = false;

			do
			{
				hoisted = false;
				for (auto& instruction : llvm::instructions(F))
				{
					llvm::LoadInst* iRef = llvm::dyn_cast<llvm::LoadInst>(&instruction);
					// If we have a load instruction, we need to see if its operand points at our global state variable
					if (iRef && iRef->getOperand(0) == bitVar.second.value)
					{
						// First load for parameter - if its not in the entry basic block, move it
						if (F.getEntryBlock().getName() != iRef->getParent()->getName())
						{
#if DEBUG_STATE_SQUASH
							std::cerr << "==" << F.getName().str() << " " << iRef->getName().str() << std::endl;
							std::cerr << "BB" << F.getEntryBlock().getName().str() << " " << iRef->getParent()->getName().str() << std::endl;
							std::cerr << "Needs Hoist" << std::endl;
#endif
							iRef->removeFromParent();
							iRef->insertBefore(hoistBefore);
							hoisted = true;
							break;
						}
					}
				}
			} while (hoisted);

			// Do state squasher
			for (auto& instruction : llvm::instructions(F))
			{
				llvm::LoadInst* iRef = llvm::dyn_cast<llvm::LoadInst>(&instruction);
				llvm::StoreInst* sRef = llvm::dyn_cast<llvm::StoreInst>(&instruction);
				// If we have a load instruction, we need to see if its operand points at our global state variable
				if (iRef && iRef->getOperand(0) == bitVar.second.value)
				{
					if (foundReference)
					{
#if DEBUG_STATE_SQUASH
						std::cerr << "--" << F.getName().str() << " " << iRef->getName().str() << std::endl;
#endif
						// Replace all uses of this instruction with original result,
						// I don't remove the current instruction - its upto a later pass to remove it
						iRef->replaceAllUsesWith(foundReference);//BinaryOperator::Create (Instruction::Or,foundReference,foundReference,"",iRef));
						doneSomeWork = true;
					}
					else
					{
#if DEBUG_STATE_SQUASH
						std::cerr << "==" << F.getName().str() << " " << iRef->getName().str() << std::endl;
#endif
						foundReference = iRef;
					}
				}
				// If we have a store instruction, we need to see if its operand points at our global state variable
				if (sRef && sRef->getOperand(1) == bitVar.second.value)
				{
					foundReference = sRef->getOperand(0);
				}
			}
		}

	}
	return doneSomeWork;
}

bool InOutReferenceSquasher::runOnFunction(llvm::Function &F)
{
	bool doneSomeWork = false;

#if DEBUG_STATE_SQUASH
	std::cerr << "Running globalReferenceSquasher" << std::endl;
#endif
	doneSomeWork |= DoWork(F, ourContext);
	for (const auto& include : ourContext->m_includes)
	{
#if DEBUG_STATE_SQUASH
		std::cerr << F.getName().str() << " " << include.second->globals().size() << std::endl;
#endif
		doneSomeWork |= DoWork(F, include.second);
	}

	return doneSomeWork;
}
