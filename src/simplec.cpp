#include <cstdio>
#include <deque>
#include <set>
#include <iostream>
#include "ast.hpp"
#include "ast_context.hpp"
#include "exceptions.hpp"
#include "parser.hpp"
#include "scope.hpp"
#include "semantic.hpp"
#include "tac_codegen.hpp"
//#include "tac_local_optimizer.hpp"
//#include "x86_codegen.hpp"

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

void dumpFunction(const Function* function)
{
	std::cerr << function->name << ":" << std::endl;

	std::deque<BasicBlock*> open = {function->firstBlock};
	std::set<BasicBlock*> closed;
	while (!open.empty())
	{
		BasicBlock* block = open.front();
		open.pop_front();

		if (closed.count(block) > 0)
			continue;

		std::cerr << block->str() << ":" << std::endl;
		Instruction* inst = block->first;
		while (inst != nullptr)
		{
			std::cerr << "\t" << inst->str() << std::endl;
			inst = inst->next;
		}

		closed.insert(block);

		std::vector<BasicBlock*> successors = block->getSuccessors();
		for (auto& successor : successors)
		{
			if (closed.count(successor) == 0)
				open.push_back(successor);
		}
	}

	std::cerr << std::endl << std::endl;
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

		for (Function* function : intermediateCode.otherFunctions)
		{
			dumpFunction(function);
		}

		dumpFunction(intermediateCode.mainFunction);

		/*
		TACLocalOptimizer localOptimizer;
		localOptimizer.optimizeCode(intermediateCode);

		X86CodeGen x86Gen;
		x86Gen.generateCode(intermediateCode);
		*/
	}

	fclose(yyin);
	return return_value;
}
