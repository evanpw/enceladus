#include "asm_printer.hpp"
#include "ast.hpp"
#include "ast_context.hpp"
#include "constant_folding.hpp"
#include "context.hpp"
#include "demote_globals.hpp"
#include "exceptions.hpp"
#include "from_ssa.hpp"
#include "machine_codegen.hpp"
#include "parser.hpp"
#include "redundant_moves.hpp"
#include "reg_alloc.hpp"
#include "semantic.hpp"
#include "stack_map.hpp"
#include "tac_codegen.hpp"
#include "tac_validator.hpp"
#include "to_ssa.hpp"

#include <cstdio>
#include <iostream>

extern ProgramNode* parse();

extern void initializeLexer(const std::string& fileName);
extern void importFile(const std::string& fileName);
extern void shutdownLexer();

int main(int argc, char* argv[])
{
	if (argc < 1)
	{
		std::cerr << "Please specify a source file to compile." << std::endl;
		return 1;
	}

	initializeLexer(argv[1]);
	importFile("lib/prelude.spl");

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
		delete astContext;
		shutdownLexer();

		return 1;
	}

	shutdownLexer();


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

	DemoteGlobals demoteGlobals(tacContext);
	demoteGlobals.run();

	for (Function* function : tacContext->functions)
	{
		ToSSA toSSA(function);
		toSSA.run();

		ConstantFolding constantFolding(function);
		constantFolding.run();

		std::cerr << function->name << ":" << std::endl;
		for (BasicBlock* block : function->blocks)
		{
			std::cerr << block->str() << std::endl;
			for (Instruction* inst = block->first; inst != nullptr; inst = inst->next)
		    {
		    	std::cerr << "\t" << inst->str() << std::endl;
		    }
		}
		std::cerr << std::endl;

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

		// std::cerr << mf->name << ":" << std::endl;
		// for (MachineBB* block : mf->blocks)
		// {
		// 	std::cerr << *block << ":" << std::endl;
		// 	for (MachineInst* inst : block->instructions)
		// 	{
		// 		std::cerr << "\t" << *inst << std::endl;
		// 	}
		// }

		StackMap stackMap(mf);
		stackMap.run();

		RedundantMoves redundantMoves(mf);
		redundantMoves.run();
	}


	// Convert the machine code to text output
	AsmPrinter asmPrinter(std::cout);
	asmPrinter.printProgram(machineContext);

	delete machineContext;

	return 0;
}
