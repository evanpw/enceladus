#include "semantic/symbol.hpp"
#include "ast/ast.hpp"

DummySymbol::DummySymbol(const std::string& name, AstNode* node)
: Symbol(name, kDummy, node, true)
{
}

VariableSymbol::VariableSymbol(const std::string& name, AstNode* node, bool global)
: Symbol(name, kVariable, node, global)
{
}

FunctionSymbol::FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition)
: Symbol(name, kFunction, node, true)
, definition(definition)
{
}

CaptureSymbol::CaptureSymbol(const std::string& name, AstNode* node, VariableSymbol* envSymbol, size_t index)
: Symbol(name, kCapture, node, false)
, envSymbol(envSymbol), index(index)
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
: Symbol(name, kType, node, true)
{
    this->type = type;
}

TraitSymbol::TraitSymbol(const std::string& name, AstNode* node, Trait* trait, Type* traitVar, std::vector<Type*>&& typeParameters)
: Symbol(name, kTrait, node, true), typeParameters(typeParameters)
{
    assert(traitVar->isVariable() && traitVar->get<TypeVariable>()->quantified());
    this->trait = trait;
    this->traitVar = traitVar;
}

MemberSymbol::MemberSymbol(const std::string& name, Kind kind, AstNode* node, Type* parentType)
: Symbol(name, kind, node, true)
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
, traitSymbol(traitSymbol)
, trait(traitSymbol->trait)
{
}

MemberVarSymbol::MemberVarSymbol(const std::string& name, AstNode* node, Type* parentType, size_t index)
: MemberSymbol(name, kMemberVar, node, parentType)
, index(index)
{
}