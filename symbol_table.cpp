#include "ast.hpp"
#include "symbol_table.hpp"

using namespace std;

map<const char*, Symbol*> SymbolTable::symbols_;

Symbol* SymbolTable::find(const char* name)
{
	map<const char*, Symbol*>::iterator i = symbols_.find(name);
	
	if (i == symbols_.end())
	{
		return 0;
	}
	else
	{
		return i->second;
	}
}

Symbol* SymbolTable::insert(const char* name, SymbolType type, AstNode* node)
{
	Symbol* symbol = new Symbol;
	symbol->name = name;
	symbol->type = type;
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