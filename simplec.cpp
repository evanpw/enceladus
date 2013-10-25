#include <cstdio>
#include <iostream>
#include "ast.hpp"
#include "codegen.hpp"
#include "scope.hpp"
#include "semantic.hpp"
#include "string_table.hpp"

using namespace std;

extern int yyparse();
extern FILE* yyin;

ProgramNode* root;

FILE* mainFile;
bool lastFile = false;

extern int yylineno;
extern int yycolumn;

extern "C" int yywrap()
{
	if (lastFile)
	{
		return 1;
	}
	else
	{
		yyin = mainFile;

		lastFile = true;
		yylineno = 0;
		yycolumn = 0;

		return 0;
	}
}

int main(int argc, char* argv[])
{
	if (argc < 1)
	{
		std::cerr << "Please specify a source file to compile." << std::endl;
		return 1;
	}

	mainFile = fopen(argv[1], "r");
	if (mainFile == nullptr)
	{
		std::cerr << "File " << argv[1] << " not found" << std::endl;
		return 1;
	}

	yyin = fopen("prelude.spl", "r");
	if (yyin == nullptr)
	{
		std::cerr << "cannot find prelude.spl" << std::endl;
		return 1;
	}

	int return_value = 1;

	int parse_failure = yyparse();
	if (parse_failure == 0)
	{
		SemanticAnalyzer semant(root);
		bool semantic_success = semant.analyze();

		if (semantic_success)
		{
			CodeGen codegen;
			root->accept(&codegen);

			return_value = 0;
		}
	}

	delete root;
	StringTable::freeStrings();
	fclose(yyin);

	return return_value;
}
