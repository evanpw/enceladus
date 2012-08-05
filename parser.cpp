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
	
	int parse_result = yyparse();
	
	SemanticAnalyzer semant(root);
	bool semantic_result = semant.analyze();
	
	if (parse_result == 0 && semantic_result)
	{
		CodeGen codegen;
		root->accept(&codegen);
	}
	
	MemoryManager::freeNodes();
	StringTable::freeStrings();
	SymbolTable::freeSymbols();
	
	return 0;
}