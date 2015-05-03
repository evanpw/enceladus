#include <cstdio>
#include <iostream>
#include "ast.hpp"
#include "ast_context.hpp"
#include "exceptions.hpp"
#include "parser.hpp"
#include "scope.hpp"
#include "semantic.hpp"
#include "tac_codegen.hpp"
#include "tac_local_optimizer.hpp"
#include "x86_codegen.hpp"

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

	AstContext context;
	Parser parser(context);
	try
	{
		parser.parse();
	}
	catch (LexerError& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		fclose(yyin);
		return 1;
	}

	ProgramNode* root = context.root();
	SemanticAnalyzer semant(root);
	bool semantic_success = semant.analyze();

	if (semantic_success)
	{
		TACCodeGen tacGen;
		root->accept(&tacGen);

		return_value = 0;

		TACProgram& intermediateCode = tacGen.getResult();

		/*
		for (const TACFunction& function : intermediateCode.otherFunctions)
		{
			std::cerr << function.name << ":" << std::endl;

			TACInstruction* inst = function.instructions;
			while (inst != nullptr)
			{
				std::cerr << inst->str() << std::endl;
				inst = inst->next;
			}

			std::cerr << std::endl << std::endl;
		}
		*/

		TACLocalOptimizer localOptimizer;
		localOptimizer.optimizeCode(intermediateCode);

		X86CodeGen x86Gen;
		x86Gen.generateCode(intermediateCode);
	}

	fclose(yyin);
	return return_value;
}
