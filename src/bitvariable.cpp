#include "bitvariable.h"

BitVariable::BitVariable(const llvm::APInt& initialSize,unsigned initialConst)
{
	size = initialSize;
	trueSize = initialSize;
	cnst = llvm::APInt(initialSize.getLimitedValue(),initialConst);
	mask = ~cnst;
	shft = llvm::APInt(initialSize.getLimitedValue(),0);

	value = nullptr;
	mapping = nullptr;
	writeAccessor=nullptr;
	writeInput=nullptr;
	priorValue=nullptr;
	impedance=nullptr;

	pinType=0;
	fromExternal=false;
	aliased = false;
	mappingRef=false;
}
