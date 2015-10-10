#include "semantic/symbol.hpp"
#include "ast/ast.hpp"

VariableSymbol::VariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction, bool global)
: Symbol(name, kVariable, node, enclosingFunction, global)
{
}

FunctionSymbol::FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition)
: Symbol(name, kFunction, node, nullptr, true)
, definition(definition)
{
}

ConstructorSymbol::ConstructorSymbol(const std::string& name, AstNode* node, ValueConstructor* constructor, const std::vector<MemberVarSymbol*>& memberSymbols)
: FunctionSymbol(name, node, nullptr)
, constructor(constructor)
, memberSymbols(memberSymbols)
{
    isConstructor = true;

    for (MemberVarSymbol* member : memberSymbols)
    {
        member->constructorSymbol = this;
    }
}

TypeSymbol::TypeSymbol(const std::string& name, AstNode* node, Type* type)
: Symbol(name, kType, node, nullptr, true)
{
    this->type = type;
}

TraitSymbol::TraitSymbol(const std::string& name, AstNode* node, Trait* trait, Type* traitVar)
: Symbol(name, kTrait, node, nullptr, true)
{
    assert(traitVar->isVariable() && traitVar->get<TypeVariable>()->quantified());
    this->trait = trait;
    this->traitVar = traitVar;
}

MemberSymbol::MemberSymbol(const std::string& name, Kind kind, AstNode* node, Type* parentType)
: Symbol(name, kind, node, nullptr, true)
, parentType(parentType)
{
}

MethodSymbol::MethodSymbol(const std::string& name, FunctionDefNode* node, Type* parentType)
: MemberSymbol(name, kMethod, node, parentType)
, definition(node)
{
}

TraitMethodSymbol::TraitMethodSymbol(const std::string& name, AstNode* node, TraitSymbol* traitSymbol)
: MemberSymbol(name, kTraitMethod, node, traitSymbol->traitVar)
, trait(traitSymbol->trait)
{
}

MemberVarSymbol::MemberVarSymbol(const std::string& name, AstNode* node, Type* parentType, size_t index)
: MemberSymbol(name, kMemberVar, node, parentType)
, index(index)
{
}