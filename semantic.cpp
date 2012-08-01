#include <iostream>
#include "semantic.hpp"
#include "simple.tab.h"
#include "symbol_table.hpp"

using namespace std;

SemanticAnalyzer::SemanticAnalyzer(ProgramNode* root)
: root_(root)
{
}

bool SemanticAnalyzer::analyze()
{	
	SemanticPass1 pass1;
	root_->accept(&pass1);
	
	SemanticPass2 pass2;
	root_->accept(&pass1);
	
	return pass1.success() && pass2.success();
}

SemanticPass1::SemanticPass1()
: success_(true)
{
}

void SemanticPass1::visit(LabelNode* node)
{	
	const char* name = node->name();
	
	Symbol* symbol = SymbolTable::find(name);
	if (symbol != 0)
	{
		if (symbol->type == kLabel)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: repeated label \"" << name << "\"" << endl;
			  	 
			success_ = false;
		}
		else
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used variable \"" << name << "\" as a label" << endl;
			  	 
			success_ = false;
		}
	}
	else
	{	
		SymbolTable::insert(name, kLabel, node);
	}
}

void SemanticPass1::visit(VariableNode* node)
{
	const char* name = node->name();
	
	Symbol* symbol = SymbolTable::find(name);
	if (symbol != 0)
	{
		if (symbol->type == kLabel)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used label \"" << name << "\" as a variable" << endl;
			  	 
			success_ = false;
		}
	}
	else
	{
		SymbolTable::insert(name, kVariable, node);
	}
}

SemanticPass2::SemanticPass2()
: success_(true)
{
}

void SemanticPass2::visit(GotoNode* node)
{
	const char* name = node->target();
	Symbol* symbol = SymbolTable::find(name);
	
	if (symbol == 0)
	{
		cerr << "Near line " << node->location()->first_line << ", "
			 << "column " << node->location()->first_column << ": "
			 << "error: undefined goto target \"" << name << "\"" << endl;
			 
		success_ = false;
	}
	else
	{
		if (symbol->type == kVariable)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: variable name \"" << name << "\" used as target of goto" << endl;
			  	 
			success_ = false;
		}
	}
}

