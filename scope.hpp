#ifndef SCOPE_HPP
#define SCOPE_HPP

#include "symbol_table.hpp"

struct Scope
{
    SymbolTable types;
    SymbolTable symbols;
};

#endif
