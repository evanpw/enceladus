#include <cstdio>
#include <iostream>
#include "ast.hpp"
#include "codegen.hpp"
#include "memory_manager.hpp"
#include "semantic.hpp"
#include "string_table.hpp"
#include "symbol_table.hpp"

using namespace std;

extern int yyparse();
extern FILE* yyin;

ProgramNode* root;

int main(int argc, char* argv[])
{
	if (argc >= 1)
	{
		yyin = fopen(argv[1], "r");
	}
	else
	{
		yyin = stdin;
	}
	
	yyparse();
	
	SemanticPass1 visitor;
	root->accept(&visitor);
	
	SemanticPass2 visitor2;
	root->accept(&visitor2);
	
	CodeGen codegen;
	root->accept(&codegen);
	
	MemoryManager::freeNodes();
	StringTable::freeStrings();
	SymbolTable::freeSymbols();
	
	return 0;
}