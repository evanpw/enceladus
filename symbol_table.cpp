#include "ast.hpp"
#include "symbol_table.hpp"

using namespace std;

const char* kindNames[] = {"kLabel", "kVariable", "kFunction"};

map<const char*, Symbol*> SymbolTable::symbols_;

Symbol* SymbolTable::find(const char* name)
{
	map<const char*, Symbol*>::iterator i = symbols_.find(name);

	if (i == symbols_.end())
	{
		return nullptr;
	}
	else
	{
		return i->second;
	}
}

Symbol* SymbolTable::insert(const char* name, Kind kind, AstNode* node)
{
	Symbol* symbol = new Symbol;
	symbol->name = name;
	symbol->kind = kind;
	symbol->node = node;

	symbols_[name] = symbol;

	return symbol;
}

void SymbolTable::freeSymbols()
{
	map<const char*, Symbol*>::iterator i;
	for (i = symbols_.begin(); i != symbols_.end(); ++i)
	{
		delete i->second;
	}

	symbols_.clear();
}
