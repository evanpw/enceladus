#include <cstdio>
#include <iostream>
#include "ast.hpp"
#include "exceptions.hpp"
#include "tac_codegen.hpp"
#include "x86_codegen.hpp"
#include "tac_local_optimizer.hpp"
#include "scope.hpp"
#include "semantic.hpp"

using namespace std;

extern ProgramNode* parse();
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
		yylineno = 1;
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

	if (strcmp(argv[1], "--noPrelude") == 0)
	{
		lastFile = true;

		if (argc < 2)
		{
			std::cerr << "Please specify a source file to compile." << std::endl;
			return 1;
		}

		yyin = fopen(argv[2], "r");
		if (yyin == nullptr)
		{
			std::cerr << "File " << argv[1] << " not found" << std::endl;
			return 1;
		}
	}
	else
	{
		mainFile = fopen(argv[1], "r");
		if (mainFile == nullptr)
		{
			std::cerr << "File " << argv[1] << " not found" << std::endl;
			return 1;
		}

		yyin = fopen("lib/prelude.spl", "r");
		if (yyin == nullptr)
		{
			std::cerr << "cannot find prelude.spl" << std::endl;
			return 1;
		}
	}

	int return_value = 1;

	ProgramNode* root;
	try
	{
		root = parse();
	}
	catch (LexerError& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		fclose(yyin);
		return 1;
	}

	SemanticAnalyzer semant(root);
	bool semantic_success = semant.analyze();

	if (semantic_success)
	{
		TACCodeGen tacGen;
		root->accept(&tacGen);

		return_value = 0;

		TACProgram& intermediateCode = tacGen.getResult();

		/*
		for (auto& inst : intermediateCode.mainFunction.instructions)
		{
			std::cerr << inst->str() << std::endl;
		}
		*/

		TACLocalOptimizer localOptimizer;
		localOptimizer.optimizeCode(intermediateCode);

		X86CodeGen x86Gen;
		x86Gen.generateCode(intermediateCode);
	}

	delete root;
	fclose(yyin);

	return return_value;
}
