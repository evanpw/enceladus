#include <iostream>
#include "ast.hpp"
#include "memory_manager.hpp"
#include "semantic.hpp"
#include "string_table.hpp"
#include "symbol_table.hpp"

using namespace std;

extern int yyparse();

ProgramNode* root;

int main()
{
	yyparse();
	
	root->show(cout, 0);
	
	SemanticPass1 visitor;
	root->accept(&visitor);
	
	SemanticPass2 visitor2;
	root->accept(&visitor2);
	
	MemoryManager::freeNodes();
	StringTable::freeStrings();
	SymbolTable::freeSymbols();
	
	return 0;
}