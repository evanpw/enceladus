#include <cassert>
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
	root_->accept(&pass2);

	TypeChecker typeChecker;
	root_->accept(&typeChecker);

	return pass1.success() && pass2.success() && typeChecker.success();
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
		else if (symbol->type == kFunction)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used function \"" << name << "\" as a label" << endl;

			success_ = false;
		}
		else if (symbol->type == kVariable)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used variable \"" << name << "\" as a label" << endl;

			success_ = false;
		}
		else
		{
			assert(false);
		}
	}
	else
	{
		node->attachSymbol(SymbolTable::insert(name, kLabel, node));
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
		else if (symbol->type == kFunction)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used function \"" << name << "\" as a variable" << endl;

			success_ = false;
		}
	}
	else
	{
		node->attachSymbol(SymbolTable::insert(name, kVariable, node));
	}
}

void SemanticPass1::visit(FunctionDefNode* node)
{
	const char* name = node->name();

	Symbol* symbol = SymbolTable::find(name);
	if (symbol != 0)
	{
		if (symbol->type == kLabel)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used label \"" << name << "\" as function name" << endl;

			success_ = false;
		}
		else if (symbol->type == kVariable)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used variable \"" << name << "\" as function name" << endl;

			success_ = false;
		}
	}
	else
	{
		node->attachSymbol(SymbolTable::insert(name, kFunction, node));
	}
}

void SemanticPass1::visit(AssignNode* node)
{
	const char* target = node->target();

	Symbol* symbol = SymbolTable::find(target);
	if (symbol != 0)
	{
		if (symbol->type == kLabel)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used label \"" << target << "\" as target of read" << endl;

			success_ = false;
		}
		else if (symbol->type == kFunction)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: used function name \"" << target << "\" as target of read" << endl;

			success_ = false;
		}
	}
	else
	{
		node->attachSymbol(SymbolTable::insert(target, kVariable, node));
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
		else if (symbol->type == kFunction)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: function name \"" << name << "\" used as target of goto" << endl;

			success_ = false;
		}
	}
}

void SemanticPass2::visit(FunctionCallNode* node)
{
	const char* name = node->target();
	Symbol* symbol = SymbolTable::find(name);

	if (symbol == 0)
	{
		cerr << "Near line " << node->location()->first_line << ", "
			 << "column " << node->location()->first_column << ": "
			 << "error: tried to call undefined function \"" << name << "\"" << endl;

		success_ = false;
	}
	else
	{
		if (symbol->type == kVariable)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: variable name \"" << name << "\" used as target of function call" << endl;

			success_ = false;
		}
		else if (symbol->type == kLabel)
		{
			cerr << "Near line " << node->location()->first_line << ", "
			  	 << "column " << node->location()->first_column << ": "
			  	 << "error: label name \"" << name << "\" used as target of function call" << endl;

			success_ = false;
		}
	}
}

TypeChecker::TypeChecker()
: success_(true)
{
}

bool TypeChecker::typeCheck(AstNode* node, Type type)
{
	if (node->type() != type)
	{
		cerr << "Near line " << node->location()->first_line << ", "
			 << "column " << node->location()->first_column << ": "
			 << "type error: expected " << typeNames[type] << ", but got " << typeNames[node->type()] << endl;

		success_ = false;
		return false;
	}

	return true;
}

// Internal nodes
void TypeChecker::visit(ProgramNode* node)
{
	for (auto& child : node->children())
	{
		child->accept(this);
	}

	node->setType(kNone);
}

void TypeChecker::visit(NotNode* node)
{
	node->child()->accept(this);

	typeCheck(node->child(), kBool);

	node->setType(kBool);
}

void TypeChecker::visit(ComparisonNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), kInt);

	node->rhs()->accept(this);
	typeCheck(node->rhs(), kInt);

	node->setType(kBool);
}

void TypeChecker::visit(BinaryOperatorNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), kInt);

	node->rhs()->accept(this);
	typeCheck(node->rhs(), kInt);

	node->setType(kInt);
}

void TypeChecker::visit(LogicalNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), kBool);

	node->rhs()->accept(this);
	typeCheck(node->rhs(), kBool);

	node->setType(kBool);
}

void TypeChecker::visit(BlockNode* node)
{
	for (auto& child : node->children())
	{
		child->accept(this);
	}

	node->setType(kNone);
}

void TypeChecker::visit(IfNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), kBool);

	node->body()->accept(this);

	node->setType(kNone);
}

void TypeChecker::visit(IfElseNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), kBool);

	node->body()->accept(this);
	node->else_body()->accept(this);

	node->setType(kNone);
}

void TypeChecker::visit(PrintNode* node)
{
	node->expression()->accept(this);
	typeCheck(node->expression(), kInt);

	node->setType(kNone);
}

void TypeChecker::visit(ReadNode* node)
{
	node->setType(kInt);
}

void TypeChecker::visit(WhileNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), kBool);

	node->body()->accept(this);

	node->setType(kNone);
}

void TypeChecker::visit(AssignNode* node)
{
	// node->target() is a variable, and variables are always integers

	node->value()->accept(this);
	typeCheck(node->value(), kInt);
}

// Leaf nodes
void TypeChecker::visit(LabelNode* node)
{
	node->setType(kNone);
}

void TypeChecker::visit(VariableNode* node)
{
	node->setType(kInt);
}

void TypeChecker::visit(IntNode* node)
{
	node->setType(kInt);
}

void TypeChecker::visit(GotoNode* node)
{
	node->setType(kNone);
}

void TypeChecker::visit(FunctionDefNode* node)
{
	node->setType(kNone);
}

void TypeChecker::visit(FunctionCallNode* node)
{
	// All return values are integers for now
	node->setType(kInt);
}

void TypeChecker::visit(ReturnNode* node)
{
	typeCheck(node->expression(), kInt);
	node->setType(kNone);
}
