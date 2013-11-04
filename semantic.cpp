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

void SemanticPass1::visit(ProgramNode* node)
{
	Scope* scope = node->scope();

	// Create symbols for built-in functions
	FunctionSymbol* notFn = new FunctionSymbol("not", node, nullptr);
	notFn->type = &Type::Bool;
	notFn->arity = 1;
	notFn->paramTypes.push_back(&Type::Bool);
	notFn->isBuiltin = true;
	scope->insert(notFn);

	FunctionSymbol* die = new FunctionSymbol("_die", node, nullptr);
	die->type = &Type::Void;
	die->arity = 1;
	die->paramTypes.push_back(&Type::Int);
	die->isExternal = true;
	die->isForeign = true;
	scope->insert(die);

	FunctionSymbol* incref = new FunctionSymbol("_incref", node, nullptr);
	incref->type = &Type::Void;
	incref->arity = 1;
	incref->paramTypes.push_back(&Type::List);
	incref->isExternal = true;
	incref->isForeign = true;
	scope->insert(incref);

	FunctionSymbol* decref = new FunctionSymbol("_decref", node, nullptr);
	decref->type = &Type::Int;
	decref->arity = 1;
	decref->paramTypes.push_back(&Type::List);
	decref->isExternal = true;
	decref->isForeign = true;
	scope->insert(decref);

	FunctionSymbol* listDecref = new FunctionSymbol("_List_decref", node, nullptr);
	listDecref->type = &Type::Void;
	listDecref->arity = 1;
	listDecref->paramTypes.push_back(&Type::List);
	listDecref->isExternal = true;
	listDecref->isForeign = true;
	scope->insert(listDecref);

	FunctionSymbol* treeDecref = new FunctionSymbol("_Tree_decref", node, nullptr);
	treeDecref->type = &Type::Void;
	treeDecref->arity = 1;
	treeDecref->paramTypes.push_back(&Type::Tree);
	treeDecref->isExternal = true;
	treeDecref->isForeign = true;
	scope->insert(treeDecref);

	FunctionSymbol* decref_no_free = new FunctionSymbol("_decrefNoFree", node, nullptr);
	decref_no_free->type = &Type::Int;
	decref_no_free->arity = 1;
	decref_no_free->paramTypes.push_back(&Type::List);
	decref_no_free->isExternal = true;
	decref_no_free->isForeign = true;
	scope->insert(decref_no_free);

	AstVisitor::visit(node);
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
	const std::string& name = node->name();
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
		const std::string& typeName = node->typeDecl()->at(i);

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

	// Return type must be valid
	const Type* returnType = Type::lookup(node->typeDecl()->back());
	if (returnType == nullptr)
	{
		std::stringstream msg;
		msg << "unknown return type \"" << node->typeDecl()->back() << "\".";
		semanticError(node, msg.str());
		return;
	}

	FunctionSymbol* symbol = new FunctionSymbol(name, node, _enclosingFunction);
	symbol->type = returnType;
	symbol->arity = node->params().size();
	symbol->paramTypes = paramTypes;
	topScope()->insert(symbol);
	node->attachSymbol(symbol);

	enterScope(node->scope());
	_enclosingFunction = node;

	// Add symbols corresponding to the formal parameters to the
	// function's scope
	for (size_t i = 0; i < node->params().size(); ++i)
	{
		const std::string& param = node->params().at(i);
		const Type* paramType = paramTypes[i];

		VariableSymbol* paramSymbol = new VariableSymbol(param, node, node);
		paramSymbol->isParam = true;
		paramSymbol->type = paramType;
		topScope()->insert(paramSymbol);
	}

	// Recurse
	node->body()->accept(this);

	_enclosingFunction = nullptr;
	exitScope();
}

void SemanticPass1::visit(ForeignDeclNode* node)
{
	// Functions cannot be declared inside of another function
	if (_enclosingFunction != nullptr)
	{
		std::stringstream msg;
		msg << "foreign function declarations must be at top level.";

		semanticError(node, msg.str());
		return;
	}

	// The function name cannot have already been used as something else
	const std::string& name = node->name();
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

	// We currently only support 6 function arguments for foreign functions
	// (so that we only have to pass arguments in registers)
	if (node->params().size() > 6)
	{
		std::stringstream msg;
		msg << "a maximum of 6 arguments is supported for foreign functions.";
		semanticError(node, msg.str());
		return;
	}

	// Parameter types must be valid
	std::vector<const Type*> paramTypes;
	for (size_t i = 0; i < node->typeDecl()->size() - 1; ++i)
	{
		const std::string& typeName = node->typeDecl()->at(i);

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

	// Return type must be valid
	const Type* returnType = Type::lookup(node->typeDecl()->back());
	if (returnType == nullptr)
	{
		std::stringstream msg;
		msg << "unknown return type \"" << node->typeDecl()->back() << "\".";
		semanticError(node, msg.str());
		return;
	}

	FunctionSymbol* symbol = new FunctionSymbol(name, node, _enclosingFunction);
	symbol->type = returnType;
	symbol->arity = node->params().size();
	symbol->isForeign = true;
	symbol->isExternal = true;
	symbol->paramTypes = paramTypes;
	topScope()->insert(symbol);
	node->attachSymbol(symbol);
}

void SemanticPass2::visit(FunctionDefNode* node)
{
	_enclosingFunction = node;
	AstVisitor::visit(node);
	_enclosingFunction = nullptr;
}

void SemanticPass2::visit(LetNode* node)
{
	const std::string& target = node->target();

	if (topScope()->find(target) != nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << target << "\" already declared in this scope.";
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

	VariableSymbol* symbol = new VariableSymbol(target, node, _enclosingFunction);
	symbol->type = type;
	topScope()->insert(symbol);
	node->attachSymbol(symbol);

	// Recurse to children
	AstVisitor::visit(node);
}

void SemanticPass2::visit(AssignNode* node)
{
	const std::string& target = node->target();

	Symbol* symbol = searchScopes(target);
	if (symbol == nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << target << "\" is not defined.";
		semanticError(node, msg.str());
		return;
	}
	else if (symbol->kind != kVariable)
	{
		std::stringstream msg;
		msg << "target of assignment \"" << target << "\" is not a variable.";
		semanticError(node, msg.str());
	}

	node->attachSymbol(static_cast<VariableSymbol*>(symbol));

	// Recurse to children
	AstVisitor::visit(node);
}

void SemanticPass2::visit(FunctionCallNode* node)
{
	const std::string& name = node->target();
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
		FunctionSymbol* functionSymbol = static_cast<FunctionSymbol*>(symbol);
		node->attachSymbol(functionSymbol);

		if (functionSymbol->arity != node->arguments().size())
		{
			std::stringstream msg;
			msg << "call to \"" << symbol->name << "\" does not respect function arity.";
			semanticError(node, msg.str());
		}
	}

	// Recurse to children
	AstVisitor::visit(node);
}

void SemanticPass2::visit(NullaryNode* node)
{
	const std::string& name = node->name();

	Symbol* symbol = searchScopes(name);
	if (symbol != nullptr)
	{
		if (symbol->kind == kVariable || symbol->kind == kFunction)
		{
			node->attachSymbol(symbol);
		}
		else
		{
			std::stringstream msg;
			msg << "symbol \"" << name << "\" is not a variable or a function.";

			semanticError(node, msg.str());
		}
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
		typeCheck(child.get(), &Type::Void);
	}
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

	// FIXME: This should probably be done in a better way
	if (node->child()->type()->isSimple())
	{
		std::stringstream msg;
		msg << "cannot call null on simple type " << node->child()->type()->name();
		semanticError(node, msg.str());
	}

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
		typeCheck(child.get(), &Type::Void);
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
void TypeChecker::visit(NullaryNode* node)
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
	// The type of a function call is the return type of the function
	node->setType(node->symbol()->type);

	assert(node->symbol()->paramTypes.size() == node->arguments().size());
	for (size_t i = 0; i < node->arguments().size(); ++i)
	{
		AstNode* argument = node->arguments().at(i).get();
		const Type* type = node->symbol()->paramTypes.at(i);

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
