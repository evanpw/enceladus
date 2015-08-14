#include "semantic/symbol_table.hpp"
#include "ast/ast.hpp"

#include <cassert>

Symbol* SymbolTable::find(const std::string& name)
{
    auto i = symbols.find(name);

    if (i == symbols.end())
    {
        return nullptr;
    }
    else
    {
        return i->second.get();
    }
}

bool SymbolTable::contains(const Symbol* symbol) const
{
    for (auto& i : symbols)
    {
        if (i.second.get() == symbol) return true;
    }

    return false;
}

void SymbolTable::insert(Symbol* symbol)
{
    if (symbols.find(symbol->name) != symbols.end()) std::cerr << symbol->name << std::endl;
    assert(symbols.find(symbol->name) == symbols.end());

    symbols[symbol->name].reset(symbol);
}

Symbol* SymbolTable::release(const std::string& name)
{
    Symbol* symbol = symbols[name].release();

    symbols.erase(name);

    return symbol;
}
