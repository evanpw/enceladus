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
		fclose(yyin);
		yyin = mainFile;

		lastFile = true;
		yylineno = 1;
		yycolumn = 0;

		return 0;
	}
}

extern int yylex_destroy();

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

	// Translate an input file to an AST (lexer and scanner)
	AstContext* astContext = new AstContext;
	Parser parser(astContext);
	try
	{
		parser.parse();
	}
	catch (LexerError& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		fclose(yyin);
		yylex_destroy();
		return 1;
	}

	fclose(yyin);
	yylex_destroy();


	// Process and annotate the AST
	SemanticAnalyzer semant(astContext);
	if (!semant.analyze())
		return 1;


	// Convert the AST to IR code
	TACContext* tacContext = new TACContext;
	TACCodeGen tacGen(tacContext);
	tacGen.codeGen(astContext);

	delete astContext;


	// Process the IR code
	TACValidator validator(tacContext);
	assert(validator.isValid());

	for (Function* function : tacContext->functions)
	{
		ToSSA toSSA(function);
		toSSA.run();

		FromSSA fromSSA(function);
		fromSSA.run();
	}


	// Convert the IR code to abstract machine code
	MachineContext* machineContext = new MachineContext;

	for (Function* function : tacContext->functions)
	{
		MachineCodeGen codeGen(machineContext, function);
	}

	for (Value* externFunction : tacContext->externs)
	{
		machineContext->externs.push_back(externFunction->name);
	}
	machineContext->externs.push_back("ccall");

	for (auto& item : tacContext->staticStrings)
	{
		machineContext->staticStrings.emplace_back(item.first->name, item.second);
	}

	for (Value* global : tacContext->globals)
	{
		machineContext->globals.push_back(global->name);
	}

	delete tacContext;


	// Process the abstract machine code and make it concrete
	for (MachineFunction* mf : machineContext->functions)
	{
		RegAlloc regAlloc(mf);
		regAlloc.run();

		RedundantMoves redundantMoves(mf);
		redundantMoves.run();
	}


	// Convert the machine code to text output
	AsmPrinter asmPrinter(std::cout);
	asmPrinter.printProgram(machineContext);

	delete machineContext;

	return 0;
}
