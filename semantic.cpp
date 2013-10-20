#include <cassert>
#include <iostream>
#include <sstream>
#include "scope.hpp"
#include "semantic.hpp"
#include "simple.tab.h"

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

void SemanticBase::semanticError(AstNode* node, const std::string& msg)
{
	std::cerr << "Near line " << node->location()->first_line << ", "
	  	      << "column " << node->location()->first_column << ": "
	  	      << "error: " << msg << std::endl;

	success_ = false;
}

void SemanticPass1::visit(FunctionDefNode* node)
{
	if (_enclosingFunction != nullptr)
	{
		std::stringstream msg;
		msg << "functions cannot be nested.";

		semanticError(node, msg.str());
		return;
	}

	const char* name = node->name();

	Symbol* symbol = searchScopes(name);
	if (symbol != nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << name << "\" is already defined.";
		semanticError(node, msg.str());

		return;
	}
	else
	{
		Symbol* symbol = new Symbol(name, kFunction, node, _enclosingFunction);
		topScope()->insert(symbol);
		node->attachSymbol(symbol);
	}

	enterScope(node->scope());
	_enclosingFunction = node;

	// Add symbols corresponding to the formal parameters to the
	// function's scope
	for (const char* param : node->params())
	{
		Symbol* paramSymbol = new Symbol(param, kVariable, node, node, true);
		topScope()->insert(paramSymbol);
	}

	// Recurse
	AstVisitor::visit(node);

	_enclosingFunction = nullptr;
	exitScope();
}

void SemanticPass1::visit(AssignNode* node)
{
	const char* target = node->target();

	Symbol* symbol = searchScopes(target);
	if (symbol != nullptr)
	{
		if (symbol->kind != kVariable)
		{
			std::stringstream msg;
			msg << "symbol \"" << symbol->name << "\" is not a variable.";
			semanticError(node, msg.str());

			return;
		}
	}
	else
	{
		symbol = new Symbol(target, kVariable, node, _enclosingFunction);
		topScope()->insert(symbol);
	}

	node->attachSymbol(symbol);

	// Recurse to children
	AstVisitor::visit(node);
}

void SemanticPass2::visit(FunctionCallNode* node)
{
	const char* name = node->target();
	Symbol* symbol = searchScopes(name);

	if (symbol == nullptr)
	{
		std::stringstream msg;
		msg << "function \"" << name << "\" is not defined.";

		semanticError(node, msg.str());
	}
	else if (symbol->kind != kFunction)
	{
		std::stringstream msg;
		msg << "target of function call \"" << symbol->name << "\" is not a function.";

		semanticError(node, msg.str());
	}

	// Recurse to children
	AstVisitor::visit(node);
}

void SemanticPass2::visit(VariableNode* node)
{
	const char* name = node->name();

	Symbol* symbol = searchScopes(name);
	if (symbol != nullptr)
	{
		if (symbol->kind != kVariable)
		{
			std::stringstream msg;
			msg << "symbol \"" << name << "\" is not a variable.";

			semanticError(node, msg.str());
		}

		node->attachSymbol(symbol);
	}
	else
	{
		std::stringstream msg;
		msg << "variable \"" << name << "\" is not defined in this scope.";

		semanticError(node, msg.str());
	}
}

void TypeChecker::typeCheck(AstNode* node, Type type)
{
	if (node->type() != type)
	{
		std::stringstream msg;
		msg << "expected type " << typeNames[type] << ", but got " << typeNames[node->type()];

		semanticError(node, msg.str());
	}
}

// Internal nodes
void TypeChecker::visit(ProgramNode* node)
{
	for (auto& child : node->children())
	{
		child->accept(this);
	}
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
}

void TypeChecker::visit(IfNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), kBool);

	node->body()->accept(this);
}

void TypeChecker::visit(IfElseNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), kBool);

	node->body()->accept(this);
	node->else_body()->accept(this);
}

void TypeChecker::visit(PrintNode* node)
{
	node->expression()->accept(this);
	typeCheck(node->expression(), kInt);
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
}

void TypeChecker::visit(AssignNode* node)
{
	// node->target() is a variable, and variables are always integers

	node->value()->accept(this);
	typeCheck(node->value(), kInt);
}

// Leaf nodes
void TypeChecker::visit(VariableNode* node)
{
	node->setType(kInt);
}

void TypeChecker::visit(IntNode* node)
{
	node->setType(kInt);
}

void TypeChecker::visit(FunctionCallNode* node)
{
	// All return values are integers for now
	node->setType(kInt);

	for (auto& argument : node->arguments())
	{
		// All function arguments are integers also
		argument.get()->accept(this);
		typeCheck(argument.get(), kInt);
	}
}

void TypeChecker::visit(ReturnNode* node)
{
	node->expression()->accept(this);

	typeCheck(node->expression(), kInt);
}
