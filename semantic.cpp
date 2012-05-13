#include <iostream>
#include "semantic.hpp"
#include "simple.tab.h"
#include "symbol_table.hpp"

using namespace std;

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
		}
		else
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used variable \"" << name << "\" as a label" << endl;
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
		}
	}
	else
	{
		SymbolTable::insert(name, kVariable, node);
	}
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
	}
	else
	{
		if (symbol->type == kVariable)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: variable name \"" << name << "\" used as target of goto" << endl;
		}
	}
}

