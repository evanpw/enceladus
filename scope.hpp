#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <map>
#include <memory>
#include "symbol.hpp"

class Scope
{
public:
	// Returns 0 if the symbol is not found in the symbol table
	Symbol* find(const char* name);

	bool contains(const Symbol* symbol) const;

	void insert(Symbol* symbol);

	const std::map<const char*, std::unique_ptr<Symbol>>& symbols() { return symbols_; }

private:
	std::map<const char*, std::unique_ptr<Symbol>> symbols_;
};

#endif
