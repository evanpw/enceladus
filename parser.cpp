#include <cstdio>
#include <iostream>
#include "ast.hpp"
#include "codegen.hpp"
#include "semantic.hpp"
#include "string_table.hpp"
#include "symbol_table.hpp"

using namespace std;

extern int yyparse();
extern FILE* yyin;

ProgramNode* root;

int main(int argc, char* argv[])
{
	if (argc < 1)
	{
		std::cerr << "Please specify a source file to compile." << std::endl;
		return 1;
	}

	yyin = fopen(argv[1], "r");

	int parse_result = yyparse();

	SemanticAnalyzer semant(root);
	bool semantic_result = semant.analyze();

	if (parse_result == 0 && semantic_result)
	{
		CodeGen codegen;
		root->accept(&codegen);
	}

	delete root;
	StringTable::freeStrings();
	SymbolTable::freeSymbols();

	fclose(yyin);
	return 0;
}
