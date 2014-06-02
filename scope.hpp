#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <unordered_map>
#include <memory>
#include "symbol.hpp"

class Scope
{
public:
	// Returns nullptr if the symbol is not found in the symbol table
	Symbol* find(const std::string& name);

	bool contains(const Symbol* symbol) const;

	void insert(Symbol* symbol);
    Symbol* release(const std::string& name);

	std::unordered_map<std::string, std::unique_ptr<Symbol>> symbols;
};

#endif
