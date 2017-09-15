#include "yyltype.h"
#include "ast.h"

#include "generator.h"	// Todo refactor away

#include <llvm/ADT/APInt.h>

bool COperandPartial::isStringReturnable() const
{
	for (int a=0;a<operands.size();a++)
	{
		if (operands[a]->isStringReturnable())
		{
			return true;
		}
	}

	return false;
}

const CString* COperandPartial::GetString(CodeGenContext& context,unsigned num,unsigned slot) const
{
	// BUGFIX: string generation params are designed to be forward iterated, but the partial loops
	//go backwards. This fixes the problem

	int a;
	int cnt=0;

	for (a=0;a<operands.size();a++)
	{
		if (operands[a]->isStringReturnable())
		{
			cnt++;
		}
	}

	slot=(cnt-slot)-1;
	for (a=operands.size()-1;a>=0;a--)
	{
		unsigned tNum=num % operands[a]->GetNumComputableConstants(context);
		const CString* temp= operands[a]->GetString(context,tNum,slot);
		if (temp!=nullptr)
		{
			if (slot == 0)
			{
				return temp;
			}
			slot--;
		}
		num/=operands[a]->GetNumComputableConstants(context);
	}

	return nullptr;
}

void COperandPartial::DeclareLocal(CodeGenContext& context,unsigned num)
{
	for (int a=operands.size()-1;a>=0;a--)
	{
		unsigned tNum=num % operands[a]->GetNumComputableConstants(context);
		operands[a]->DeclareLocal(context,tNum);
		num/=operands[a]->GetNumComputableConstants(context);
	}
}

llvm::APInt COperandPartial::GetComputableConstant(CodeGenContext& context,unsigned num) const
{
	llvm::APInt result;
	bool firstTime=true;

	if (operands.size()==0)
	{
		return context.gContext.ReportError(result, EC_InternalError, YYLTYPE(), "Illegal (0) number of operands for computable constant");
	}
	for (int a=operands.size()-1;a>=0;a--)
	{
		unsigned tNum=num % operands[a]->GetNumComputableConstants(context);
		llvm::APInt temp = operands[a]->GetComputableConstant(context,tNum);
		num/=operands[a]->GetNumComputableConstants(context);
		if (firstTime)
		{
			result=temp;
			firstTime=false;
		}
		else
		{
			unsigned curBitWidth = result.getBitWidth();

			result=result.zext(result.getBitWidth()+temp.getBitWidth());
			temp=temp.zext(result.getBitWidth());
			temp=temp.shl(curBitWidth);
			result=result | temp;
		}
	}

	return result;
}

unsigned COperandPartial::GetNumComputableConstants(CodeGenContext& context) const
{
	unsigned result=1;

	for (int a=0;a<operands.size();a++)
	{
		result*=operands[a]->GetNumComputableConstants(context);
	}

	return result;
}
