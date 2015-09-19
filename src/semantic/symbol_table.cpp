#include "semantic/symbol_table.hpp"
#include "ast/ast.hpp"

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

MemberSymbol* SymbolTable::createMemberSymbol(const std::string& name, AstNode* node)
{
    MemberSymbol* symbol = new MemberSymbol(name, node);
    insert(symbol);
    return symbol;
}

MethodSymbol* SymbolTable::createMethodSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition, Type* parentType)
{
    MethodSymbol* symbol = new MethodSymbol(name, node, definition, parentType);
    insertMethod(symbol);
    return symbol;
}

TypeSymbol* SymbolTable::createTypeSymbol(const std::string& name, AstNode* node, Type* type)
{
    TypeSymbol* symbol = new TypeSymbol(name, node, type);
    insert(symbol, SymbolTable::TYPE);
    return symbol;
}

TypeConstructorSymbol* SymbolTable::createTypeConstructorSymbol(const std::string& name, AstNode* node, TypeConstructor* typeConstructor)
{
    TypeConstructorSymbol* symbol = new TypeConstructorSymbol(name, node, typeConstructor);
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

void SymbolTable::findMethods(const std::string& name, std::vector<MethodSymbol*>& result)
{
    result.clear();

    auto i = _methods.find(name);
    if (i != _methods.end())
    {
        result = i->second;
    }
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

void SymbolTable::insertMethod(MethodSymbol* symbol)
{
    if (symbol->name != "_")
        _methods[symbol->name].push_back(symbol);
}
