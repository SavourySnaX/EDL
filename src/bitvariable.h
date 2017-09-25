#include "yyltype.h"

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Value.h>

class CMappingDeclaration;

#pragma once

class BitVariable
{
public:
	BitVariable() { }
	BitVariable(const llvm::APInt& initialSize,unsigned initialConst);

	YYLTYPE					refLoc;
	llvm::APInt				arraySize;
	llvm::APInt				size;
	llvm::APInt				trueSize;
	llvm::APInt				mask;
	llvm::APInt				cnst;
	llvm::APInt				shft;
	llvm::Value*			value;
	CMappingDeclaration*	mapping;
	llvm::Instruction**		writeAccessor;
	llvm::Value*			writeInput;
	llvm::Value*			priorValue;
	llvm::Value*			impedance;
	int						pinType;
	bool					aliased;
	bool					mappingRef;
	bool					fromExternal;
};
