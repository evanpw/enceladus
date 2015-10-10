#include "semantic/symbol_table.hpp"
#include "ast/ast.hpp"
#include "semantic/types.hpp"

#include <cassert>

VariableSymbol* SymbolTable::createVariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction, bool global)
{
    VariableSymbol* symbol = new VariableSymbol(name, node, enclosingFunction, global);
    insert(symbol);
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

TraitSymbol* SymbolTable::createTraitSymbol(const std::string& name, AstNode* node, Trait* trait, Type* traitVar)
{
    TraitSymbol* symbol = new TraitSymbol(name, node, trait, traitVar);
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
            return i->second;
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

    auto i = _members.find(name);
    if (i == _members.end()) return;

    for (MemberSymbol* symbol : i->second)
    {
        if (isCompatible(parentType, instantiate(symbol->parentType)))
        {
            symbols.push_back(symbol);
        }
    }
}

MethodSymbol* SymbolTable::resolveConcreteMethod(const std::string& name, Type* objectType)
{
    auto i = _members.find(name);
    if (i == _members.end()) return nullptr;

    for (MemberSymbol* symbol : i->second)
    {
        if (symbol->kind == kMethod && isCompatible(objectType, instantiate(symbol->parentType)))
        {
            return dynamic_cast<MethodSymbol*>(symbol);
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
