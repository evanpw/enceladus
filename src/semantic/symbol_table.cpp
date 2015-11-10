#include "semantic/symbol_table.hpp"
#include "ast/ast.hpp"
#include "semantic/subtype.hpp"
#include "semantic/type_functions.hpp"

#include <cassert>

VariableSymbol* SymbolTable::createVariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction, bool global)
{
    VariableSymbol* symbol = new VariableSymbol(name, node, enclosingFunction, global);

    if (symbol->name != "_")
    {
        insert(symbol);
    }

    return symbol;
}

FunctionSymbol* SymbolTable::createFunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition)
{
    FunctionSymbol* symbol = new FunctionSymbol(name, node, definition);
    insert(symbol);
    return symbol;
}

ConstructorSymbol* SymbolTable::createConstructorSymbol(const std::string& name, AstNode* node, ValueConstructor* constructor, const std::vector<MemberVarSymbol*>& memberSymbols)
{
    ConstructorSymbol* symbol = new ConstructorSymbol(name, node, constructor, memberSymbols);
    insert(symbol);
    return symbol;
}

MethodSymbol* SymbolTable::createMethodSymbol(const std::string& name, FunctionDefNode* node, Type* parentType)
{
    MethodSymbol* symbol = new MethodSymbol(name, node, parentType);

    if (symbol->name != "_")
    {
        _members[name].push_back(symbol);
    }

    return symbol;
}

TraitMethodSymbol* SymbolTable::createTraitMethodSymbol(const std::string& name, AstNode* node, TraitSymbol* traitSymbol)
{
    TraitMethodSymbol* symbol = new TraitMethodSymbol(name, node, traitSymbol);
    assert(name != "_");
    _members[name].push_back(symbol);

    return symbol;
}

MemberVarSymbol* SymbolTable::createMemberVarSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition, Type* parentType, size_t index)
{
    MemberVarSymbol* symbol = new MemberVarSymbol(name, node, parentType, index);

    if (symbol->name != "_")
    {
        _members[name].push_back(symbol);
    }

    return symbol;
}

TypeSymbol* SymbolTable::createTypeSymbol(const std::string& name, AstNode* node, Type* type)
{
    TypeSymbol* symbol = new TypeSymbol(name, node, type);
    insert(symbol, SymbolTable::TYPE);
    return symbol;
}

TraitSymbol* SymbolTable::createTraitSymbol(const std::string& name, AstNode* node, Trait* trait, Type* traitVar, std::vector<Type*>&& typeParameters)
{
    TraitSymbol* symbol = new TraitSymbol(name, node, trait, traitVar, std::move(typeParameters));
    insert(symbol, SymbolTable::TYPE);
    return symbol;
}

DummySymbol* SymbolTable::createDummySymbol(const std::string& name, AstNode* node)
{
    DummySymbol* symbol = new DummySymbol(name, node);
    insert(symbol, SymbolTable::TYPE);
    return symbol;
}

void SymbolTable::pushScope()
{
    _scopes.push_back({});
}

void SymbolTable::popScope()
{
    _scopes.pop_back();
}

Symbol* SymbolTable::find(const std::string& name, WhichTable whichTable)
{
    auto key = std::make_pair(name, whichTable);

    for (auto scope = _scopes.rbegin(); scope != _scopes.rend(); ++scope)
    {
        auto i = scope->find(key);
        if (i != scope->end())
        {
            return i->second;
        }
    }

    return nullptr;
}

Symbol* SymbolTable::findTopScope(const std::string& name, WhichTable whichTable)
{
    auto key = std::make_pair(name, whichTable);
    auto& scope = _scopes.back();

    auto i = scope.find(key);
    if (i == scope.end())
    {
        return nullptr;
    }
    else
    {
        return i->second;
    }
}

void SymbolTable::findMembers(const std::string& name, std::vector<MemberSymbol*>& result)
{
    result.clear();

    auto i = _members.find(name);
    if (i != _members.end())
    {
        result = i->second;
    }
}

void SymbolTable::resolveMemberSymbol(const std::string& name, Type* parentType, std::vector<MemberSymbol*>& symbols)
{
    symbols.clear();

    // Never match an unconstrained type to a method
    if (parentType->isVariable() && parentType->get<TypeVariable>()->constraints().empty())
        return;

    bool matchTraits = parentType->isVariable() && parentType->get<TypeVariable>()->quantified();

    auto i = _members.find(name);
    if (i == _members.end()) return;

    for (MemberSymbol* symbol : i->second)
    {
        // Trait methods only resolve for quantified type variables, and
        // quantified type variables only resolve to trait methods
        if ((matchTraits && symbol->kind != kTraitMethod) || (!matchTraits && symbol->kind == kTraitMethod))
            continue;

        if (isSubtype(parentType, symbol->parentType))
        {
            symbols.push_back(symbol);
        }
    }
}

MethodSymbol* SymbolTable::resolveTraitInstanceMethod(const std::string& name, Type* objectType, TraitSymbol* traitSymbol)
{
    for (auto& item : traitSymbol->_instances)
    {
        if (isSubtype(objectType, item.second->type))
        {
            return item.second->methods.at(name);
        }
    }

    return nullptr;
}

Type* SymbolTable::resolveAssociatedType(const std::string& name, Type* objectType, TraitSymbol* traitSymbol)
{
    for (auto& item : traitSymbol->_instances)
    {
        TypeComparer comparer;

        if (comparer.compare(objectType, item.second->type))
        {
            Type* rawType = item.second->associatedTypes.at(name);
            return substitute(rawType, comparer.rhsSubs());
        }
    }

    return nullptr;
}

void SymbolTable::insert(Symbol* symbol, WhichTable whichTable)
{
    auto key = std::make_pair(symbol->name, whichTable);
    auto& scope = _scopes.back();

    assert(scope.find(key) == scope.end());

    _symbols.emplace_back(symbol);

    if (symbol->name != "_")
        scope.emplace(key, symbol);
}
