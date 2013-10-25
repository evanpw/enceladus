#include <cassert>
#include <cstring>
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

	if (pass1.success())
	{
		SemanticPass2 pass2;
		root_->accept(&pass2);

		if (pass2.success())
		{
			TypeChecker typeChecker;
			root_->accept(&typeChecker);

			return typeChecker.success();
		}
	}

	return false;
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
	// Functions cannot be declared inside of another function
	if (_enclosingFunction != nullptr)
	{
		std::stringstream msg;
		msg << "functions cannot be nested.";

		semanticError(node, msg.str());
		return;
	}

	// The function name cannot have already been used as something else
	const char* name = node->name();
	if (searchScopes(name) != nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << name << "\" is already defined.";
		semanticError(node, msg.str());

		return;
	}

	// Must have a type specified for each parameter + one for return type
	if (node->typeDecl()->size() != node->params().size() + 1)
	{
		std::stringstream msg;
		msg << "number of types does not match parameter list";
		semanticError(node, msg.str());
		return;
	}

	// Parameter types must be valid
	std::vector<const Type*> paramTypes;
	for (size_t i = 0; i < node->typeDecl()->size() - 1; ++i)
	{
		const char* typeName = node->typeDecl()->at(i);

		const Type* type = Type::lookup(typeName);
		if (type == nullptr)
		{
			std::stringstream msg;
			msg << "unknown parameter type \"" << typeName << "\".";
			semanticError(node, msg.str());
			return;
		}

		paramTypes.push_back(type);
	}
	node->setParamTypes(paramTypes);

	// Return type must be valid
	const Type* returnType = Type::lookup(node->typeDecl()->back());
	if (returnType == nullptr)
	{
		std::stringstream msg;
		msg << "unknown return type \"" << node->typeDecl()->back() << "\".";
		semanticError(node, msg.str());
		return;
	}

	Symbol* symbol = new Symbol(name, kFunction, node, _enclosingFunction);
	symbol->type = returnType;
	symbol->arity = node->params().size();
	topScope()->insert(symbol);
	node->attachSymbol(symbol);

	enterScope(node->scope());
	_enclosingFunction = node;

	// Add symbols corresponding to the formal parameters to the
	// function's scope
	for (size_t i = 0; i < node->params().size(); ++i)
	{
		const char* param = node->params().at(i);
		const Type* paramType = paramTypes[i];

		Symbol* paramSymbol = new Symbol(param, kVariable, node, node);
		paramSymbol->isParam = true;
		paramSymbol->type = paramType;
		topScope()->insert(paramSymbol);
	}

	// Recurse
	node->body()->accept(this);

	_enclosingFunction = nullptr;
	exitScope();
}

void SemanticPass1::visit(LetNode* node)
{
	const char* target = node->target();

	Symbol* symbol = topScope()->find(target);
	if (symbol != nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << symbol->name << "\" already declared in this scope.";
		semanticError(node, msg.str());
		return;
	}

	const Type* type = Type::lookup(node->typeDecl());
	if (type == nullptr)
	{
		std::stringstream msg;
		msg << "unknown type \"" << node->typeDecl() << "\".";
		semanticError(node, msg.str());
		return;
	}

	symbol = new Symbol(target, kVariable, node, _enclosingFunction);
	symbol->type = type;
	topScope()->insert(symbol);
	node->attachSymbol(symbol);

	// Recurse to children
	AstVisitor::visit(node);
}

void SemanticPass2::visit(AssignNode* node)
{
	const char* target = node->target();

	Symbol* symbol = searchScopes(target);
	if (symbol == nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << target << "\" is not defined.";
		semanticError(node, msg.str());
		return;
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
	else
	{
		node->attachSymbol(symbol);

		if (symbol->arity != node->arguments().size())
		{
			std::stringstream msg;
			msg << "call to \"" << symbol->name << "\" does not respect function arity.";
			semanticError(node, msg.str());
		}
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

void TypeChecker::typeCheck(AstNode* node, const Type* type)
{
	if (node->type() != type)
	{
		std::stringstream msg;
		msg << "expected type " << type->name() << ", but got " << node->type()->name();

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

	typeCheck(node->child(), &Type::Bool);

	node->setType(&Type::Bool);
}

void TypeChecker::visit(ComparisonNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), &Type::Int);

	node->rhs()->accept(this);
	typeCheck(node->rhs(), &Type::Int);

	node->setType(&Type::Bool);
}

void TypeChecker::visit(BinaryOperatorNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), &Type::Int);

	node->rhs()->accept(this);
	typeCheck(node->rhs(), &Type::Int);

	node->setType(&Type::Int);
}

void TypeChecker::visit(ConsNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), &Type::Int);

	node->rhs()->accept(this);
	typeCheck(node->rhs(), &Type::List);

	node->setType(&Type::List);
}

void TypeChecker::visit(HeadNode* node)
{
	node->child()->accept(this);
	typeCheck(node->child(), &Type::List);

	node->setType(&Type::Int);
}

void TypeChecker::visit(TailNode* node)
{
	node->child()->accept(this);
	typeCheck(node->child(), &Type::List);

	node->setType(&Type::List);
}

void TypeChecker::visit(NullNode* node)
{
	node->child()->accept(this);
	typeCheck(node->child(), &Type::List);

	node->setType(&Type::Bool);
}

void TypeChecker::visit(LogicalNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), &Type::Bool);

	node->rhs()->accept(this);
	typeCheck(node->rhs(), &Type::Bool);

	node->setType(&Type::Bool);
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
	typeCheck(node->condition(), &Type::Bool);

	node->body()->accept(this);
}

void TypeChecker::visit(IfElseNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), &Type::Bool);

	node->body()->accept(this);
	node->else_body()->accept(this);
}

void TypeChecker::visit(PrintNode* node)
{
	node->expression()->accept(this);
	typeCheck(node->expression(), &Type::Int);
}

void TypeChecker::visit(ReadNode* node)
{
	node->setType(&Type::Int);
}

void TypeChecker::visit(WhileNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), &Type::Bool);

	node->body()->accept(this);
}

void TypeChecker::visit(AssignNode* node)
{
	node->value()->accept(this);
	typeCheck(node->value(), node->symbol()->type);
}

// Leaf nodes
void TypeChecker::visit(VariableNode* node)
{
	node->setType(node->symbol()->type);
}

void TypeChecker::visit(IntNode* node)
{
	node->setType(&Type::Int);
}

void TypeChecker::visit(BoolNode* node)
{
	node->setType(&Type::Bool);
}

void TypeChecker::visit(NilNode* node)
{
	node->setType(&Type::List);
}

void TypeChecker::visit(FunctionCallNode* node)
{
	FunctionDefNode* function = dynamic_cast<FunctionDefNode*>(node->symbol()->node);

	// The type of a function call is the return type of the function
	node->setType(node->symbol()->type);

	assert(function->paramTypes().size() == node->arguments().size());
	for (size_t i = 0; i < node->arguments().size(); ++i)
	{
		AstNode* argument = node->arguments().at(i).get();
		const Type* type = function->paramTypes().at(i);

		argument->accept(this);
		typeCheck(argument, type);
	}
}

void TypeChecker::visit(ReturnNode* node)
{
	// This isn't really type checking, but this is the easiest place to put this check.
	if (_enclosingFunction == nullptr)
	{
		std::stringstream msg;
		msg << "Cannot return from top level.";
		semanticError(node, msg.str());

		return;
	}

	node->expression()->accept(this);

	// Value of expression must equal the return type of the enclosing function.
	typeCheck(node->expression(), _enclosingFunction->symbol()->type);
}

void TypeChecker::visit(FunctionDefNode* node)
{
	enterScope(node->scope());
	_enclosingFunction = node;

	// Recurse
	node->body()->accept(this);

	_enclosingFunction = nullptr;
	exitScope();
}
