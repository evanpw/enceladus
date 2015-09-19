#include "semantic/symbol.hpp"

VariableSymbol::VariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction, bool global)
: Symbol(name, kVariable, node, enclosingFunction, global)
{
}

FunctionSymbol::FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition)
: Symbol(name, kFunction, node, nullptr, true)
, definition(definition)
{
}

TypeSymbol::TypeSymbol(const std::string& name, AstNode* node, Type* type)
: Symbol(name, kType, node, nullptr, true)
{
    this->type = type;
}

TypeConstructorSymbol::TypeConstructorSymbol(const std::string& name, AstNode* node, TypeConstructor* typeConstructor)
: Symbol(name, kTypeConstructor, node, nullptr, true)
, typeConstructor(typeConstructor)
{
}

MemberSymbol::MemberSymbol(const std::string& name, AstNode* node)
: Symbol(name, kMember, node, nullptr, true)
{
}

MethodSymbol::MethodSymbol(const std::string& name, AstNode* node, FunctionDeclNode* declaration, TraitDefNode* traitNode)
: Symbol(name, kMethod, node, nullptr, true)
, declaration(declaration)
, traitNode(traitNode)
{
}