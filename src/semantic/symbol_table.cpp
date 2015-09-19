#include "semantic/symbol_table.hpp"
#include "ast/ast.hpp"

#include <cassert>

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

void SymbolTable::insert(Symbol* symbol, WhichTable whichTable)
{
    auto key = std::make_pair(symbol->name, whichTable);
    auto& scope = _scopes.back();

    assert(scope.find(key) == scope.end());

    _symbols.emplace_back(symbol);
    scope.emplace(key, symbol);
}
