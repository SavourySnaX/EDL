#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

extern void PrintErrorFromLocation(const YYLTYPE &location, const char *errorstring, ...);		// Todo refactor away

void COperandMapping::DeclareLocal(CodeGenContext& context, unsigned num)
{
	context.locals()[ident.name] = GetBitVariable(context, num);
}

BitVariable COperandMapping::GetBitVariable(CodeGenContext& context, unsigned num)
{
	BitVariable mappingVariable;

	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		PrintErrorFromLocation(ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
		context.errorFlagged = true;
		return mappingVariable;
	}

	CMapping* mapping = context.m_mappings[ident.name]->mappings[num];

	if (mapping->expr.IsIdentifierExpression())
	{
		// If the expression in a mapping is a an identifier alone, then we can create a true local variable that maps directly to it, this allows
		// assignments to work. (its done by copying the variable declaration resulting from looking up the identifier.)

		CIdentifier* identifier = (CIdentifier*)&mapping->expr;
		if (!context.LookupBitVariable(mappingVariable, identifier->module, identifier->name, identifier->modLoc, identifier->nameLoc))
		{
			return mappingVariable;
		}
	}
	else
	{
		// Not an identifier, so we must create an expression map (this limits the mapping to only being useful for a limited set of things, (but is an accepted part of the language!)

		mappingVariable.mappingRef = true;
		mappingVariable.mapping = &mapping->expr;
	}
	return mappingVariable;
}

llvm::APInt COperandMapping::GetComputableConstant(CodeGenContext& context, unsigned num) const
{
	llvm::APInt error;

	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		PrintErrorFromLocation(ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
		context.errorFlagged = true;
		return error;
	}

	return context.m_mappings[ident.name]->mappings[num]->selector.getAPInt();
}

unsigned COperandMapping::GetNumComputableConstants(CodeGenContext& context) const
{
	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		PrintErrorFromLocation(ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
		context.errorFlagged = true;
		return 0;
	}

	return context.m_mappings[ident.name]->mappings.size();
}

const CString* COperandMapping::GetString(CodeGenContext& context, unsigned num, unsigned slot) const
{
	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		PrintErrorFromLocation(ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
		context.errorFlagged = true;
		return nullptr;
	}

	return &context.m_mappings[ident.name]->mappings[num]->label;
}
