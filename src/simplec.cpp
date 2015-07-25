#include "asm_printer.hpp"
#include "ast.hpp"
#include "ast_context.hpp"
#include "context.hpp"
#include "exceptions.hpp"
#include "machine_codegen.hpp"
#include "parser.hpp"
#include "reg_alloc.hpp"
#include "semantic.hpp"
#include "redundant_moves.hpp"
#include "tac_codegen.hpp"
#include "tac_validator.hpp"
#include "from_ssa.hpp"
#include "to_ssa.hpp"

#include <cstdio>
#include <iostream>

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

	AstContext astContext;
	Parser parser(astContext);
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

	ProgramNode* root = astContext.root();
	SemanticAnalyzer semant(root, &astContext);
	bool semantic_success = semant.analyze();
	if (semantic_success)
	{
		TACContext* tacContext = new TACContext;
		TACCodeGen tacGen(tacContext);
		root->accept(&tacGen);

		return_value = 0;

		TACValidator validator(tacContext);
		assert(validator.isValid());

		MachineContext machineContext;

		for (Function* function : tacContext->functions)
		{
			ToSSA toSSA(function);
			toSSA.run();

			FromSSA fromSSA(function);
			fromSSA.run();

			// MachineCodeGen codeGen(&machineContext, function);
			// MachineFunction* mf = codeGen.getResult();

			// RegAlloc regAlloc(mf);
			// regAlloc.run();

			// RedundantMoves redundantMoves(mf);
			// redundantMoves.run();
		}

		// for (Value* externFunction : tacContext->externs)
		// {
		// 	machineContext.externs.push_back(externFunction->name);
		// }
		// machineContext.externs.push_back("ccall");

		// for (auto& item : tacContext->staticStrings)
		// {
		// 	machineContext.staticStrings.emplace_back(item.first->name, item.second);
		// }

		// for (Value* global : tacContext->globals)
		// {
		// 	machineContext.globals.push_back(global->name);
		// }

		delete tacContext;

		// AsmPrinter asmPrinter(std::cout);
		// asmPrinter.printProgram(&machineContext);
	}

	fclose(yyin);
	return return_value;
}
