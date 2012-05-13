#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <map>

class AstNode;

enum SymbolType {kLabel, kVariable};

struct Symbol
{
	// This name MUST BE stored in the string table. The symbol table is indexed by pointer,
	// not by string, so there cannot be multiple copies of the same string or bad things will
	// happen.
	const char* name;
	
	// Label or variable?
	SymbolType type;
	
	// For a label, the node at which the label is declared. For a variable, null
	AstNode* node;
};

class SymbolTable
{
public:
	// Returns 0 if the symbol is not found in the symbol table
	static Symbol* find(const char* name);
	
	static Symbol* insert(const char* name, SymbolType type, AstNode* node = 0);
	static void freeSymbols();
	
private:
	SymbolTable();
	
	static std::map<const char*, Symbol*> symbols_;
};

#endif
