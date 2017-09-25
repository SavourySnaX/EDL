#include "yyltype.h"
#include "ast.h"
#include "bitvariable.h"

#include "generator.h"	// Todo refactor away

void COperandMapping::DeclareLocal(CodeGenContext& context, unsigned num)
{
	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		context.gContext.ReportError(nullptr,EC_ErrorAtLocation,ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
		return;
	}

	BitVariable temp(context.m_mappings[ident.name]->size.getAPInt(),0);

	temp.value = arg;
	temp.value->setName(ident.name+"idx");
	context.locals()[ident.name+"idx"] = temp;

	BitVariable mappingRef;

	mappingRef.mappingRef = true;
	mappingRef.mapping = context.m_mappings[ident.name];
	
	context.locals()[ident.name] = mappingRef;
}

llvm::APInt COperandMapping::GetComputableConstant(CodeGenContext& context, unsigned num) const
{
	llvm::APInt error;

	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		return context.gContext.ReportError(error, EC_ErrorAtLocation, ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
	}

	return context.m_mappings[ident.name]->mappings[num]->selector.getAPInt();
}

unsigned COperandMapping::GetNumComputableConstants(CodeGenContext& context) const
{
	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		return context.gContext.ReportError(0, EC_ErrorAtLocation, ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
	}

	return context.m_mappings[ident.name]->mappings.size();
}

void COperandMapping::GetArgTypes(CodeGenContext& context, std::vector<llvm::Type*>& argVector) const
{
	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		context.gContext.ReportError(nullptr, EC_ErrorAtLocation, ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
		return;
	}

	argVector.push_back(context.getIntType(context.m_mappings[ident.name]->size.getAPInt().getLimitedValue()));
}

void COperandMapping::GetArgs(CodeGenContext& context, std::vector<llvm::Value*>& argVector,unsigned num) const
{
	argVector.push_back(context.getConstantInt(GetComputableConstant(context, num)));
}

const CString* COperandMapping::GetString(CodeGenContext& context, unsigned num, unsigned slot) const
{
	if (context.m_mappings.find(ident.name) == context.m_mappings.end())
	{
		return context.gContext.ReportError(nullptr, EC_ErrorAtLocation, ident.nameLoc, "Undeclared mapping : %s", ident.name.c_str());
	}

	return &context.m_mappings[ident.name]->mappings[num]->label;
}
