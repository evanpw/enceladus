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
	TypeTable* typeTable = node->typeTable();

	std::shared_ptr<Type> listOfInts = ConstructedType::create(typeTable->getTypeConstructor("List"), {typeTable->getBaseType("Int")});

	// Create symbols for built-in functions
	FunctionSymbol* notFn = new FunctionSymbol("not", node, nullptr);
	notFn->type = typeTable->getBaseType("Bool");
	notFn->arity = 1;
	notFn->paramTypes.push_back(typeTable->getBaseType("Bool"));
	notFn->isBuiltin = true;
	scope->insert(notFn);

	FunctionSymbol* head = new FunctionSymbol("head", node, nullptr);
	head->type = typeTable->getBaseType("Int");
	head->arity = 1;
	head->paramTypes.push_back(listOfInts);
	head->isBuiltin = true;
	scope->insert(head);

	FunctionSymbol* tail = new FunctionSymbol("tail", node, nullptr);
	tail->type = listOfInts;
	tail->arity = 1;
	tail->paramTypes.push_back(listOfInts);
	tail->isBuiltin = true;
	scope->insert(tail);

	FunctionSymbol* die = new FunctionSymbol("_die", node, nullptr);
	die->type = typeTable->getBaseType("Unit");
	die->arity = 1;
	die->paramTypes.push_back(typeTable->getBaseType("Unit"));
	die->isExternal = true;
	die->isForeign = true;
	scope->insert(die);

	FunctionSymbol* incref = new FunctionSymbol("_incref", node, nullptr);
	incref->type = typeTable->getBaseType("Unit");
	incref->arity = 1;
	incref->paramTypes.push_back(listOfInts);
	incref->isExternal = true;
	incref->isForeign = true;
	scope->insert(incref);

	FunctionSymbol* decref = new FunctionSymbol("_decref", node, nullptr);
	decref->type = typeTable->getBaseType("Int");
	decref->arity = 1;
	decref->paramTypes.push_back(listOfInts);
	decref->isExternal = true;
	decref->isForeign = true;
	scope->insert(decref);

	FunctionSymbol* decref_no_free = new FunctionSymbol("_decrefNoFree", node, nullptr);
	decref_no_free->type = typeTable->getBaseType("Int");
	decref_no_free->arity = 1;
	decref_no_free->paramTypes.push_back(listOfInts);
	decref_no_free->isExternal = true;
	decref_no_free->isForeign = true;
	scope->insert(decref_no_free);

	AstVisitor::visit(node);
}

void SemanticPass1::visit(DataDeclaration* node)
{
	// Data declarations cannot be local
	if (_enclosingFunction != nullptr)
	{
		std::stringstream msg;
		msg << "data declarations must be at top level.";

		semanticError(node, msg.str());
		return;
	}

	// The data type and constructor name cannot have already been used for something
	const std::string& typeName = node->name();
	if (searchScopes(typeName) != nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << typeName << "\" is already defined.";
		semanticError(node, msg.str());
		return;
	}

	const std::string& constructorName = node->constructor()->name();
	if (searchScopes(constructorName) != nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << typeName << "\" is already defined.";
		semanticError(node, msg.str());
		return;
	}

	// All of the constructor members must refer to already-declared types
	std::vector<std::shared_ptr<Type>> memberTypes;
	for (auto& memberTypeName : node->constructor()->members())
	{
		std::shared_ptr<Type> memberType = typeTable_->nameToType(memberTypeName.get());
		if (!memberType)
		{
			std::stringstream msg;
			msg << "unknown member type \"" << memberType << "\".";
			semanticError(node, msg.str());
			return;
		}

		memberTypes.push_back(memberType);
	}
	node->constructor()->setMemberTypes(memberTypes);

	// Actually create the type
	std::shared_ptr<Type> newType = BaseType::create(node->name());
	typeTable_->insert(typeName, newType);

	ValueConstructor* valueConstructor = new ValueConstructor(node->constructor()->name(), memberTypes);
	node->attachConstructor(valueConstructor);
	newType->addValueConstructor(valueConstructor);

	// Create a symbol for the constructor
	FunctionSymbol* symbol = new FunctionSymbol(constructorName, node, nullptr);
	symbol->type = newType;
	symbol->arity = memberTypes.size();
	symbol->paramTypes = memberTypes;
	topScope()->insert(symbol);
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
	std::vector<std::shared_ptr<Type>> paramTypes;
	for (size_t i = 0; i < node->typeDecl()->size() - 1; ++i)
	{
		TypeName* typeName = node->typeDecl()->at(i).get();

		std::shared_ptr<Type> type = typeTable_->nameToType(typeName);
		if (!type)
		{
			std::stringstream msg;
			msg << "unknown parameter type \"" << typeName << "\".";
			semanticError(node, msg.str());
			return;
		}

		paramTypes.push_back(type);
	}

	// Return type must be valid
	std::shared_ptr<Type> returnType = typeTable_->nameToType(node->typeDecl()->back().get());
	if (!returnType)
	{
		std::stringstream msg;
		msg << "unknown return type \"" << node->typeDecl()->back()->name() << "\".";
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

		VariableSymbol* paramSymbol = new VariableSymbol(param, node, node);
		paramSymbol->isParam = true;
		paramSymbol->type = paramTypes[i];
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

	// If parameters names are given, must have a type specified for
	// each parameter + one for return type
	if (node->params().size() != 0 &&
		node->typeDecl()->size() != node->params().size() + 1)
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
	std::vector<std::shared_ptr<Type>> paramTypes;
	for (size_t i = 0; i < node->typeDecl()->size() - 1; ++i)
	{
		TypeName* typeName = node->typeDecl()->at(i).get();

		std::shared_ptr<Type> type = typeTable_->nameToType(typeName);
		if (!type)
		{
			std::stringstream msg;
			msg << "unknown parameter type \"" << typeName << "\".";
			semanticError(node, msg.str());
			return;
		}

		paramTypes.push_back(type);
	}

	// Return type must be valid
	std::shared_ptr<Type> returnType = typeTable_->nameToType(node->typeDecl()->back().get());
	if (!returnType)
	{
		std::stringstream msg;
		msg << "unknown return type \"" << node->typeDecl()->back()->name() << "\".";
		semanticError(node, msg.str());
		return;
	}

	FunctionSymbol* symbol = new FunctionSymbol(name, node, _enclosingFunction);
	symbol->type = returnType;
	symbol->arity = node->typeDecl()->size() - 1;
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
	// Visit children. Do this first so that we can't have recursive definitions.
	AstVisitor::visit(node);

	const std::string& target = node->target();
	if (topScope()->find(target) != nullptr)
	{
		std::stringstream msg;
		msg << "symbol \"" << target << "\" already declared in this scope.";
		semanticError(node, msg.str());
		return;
	}

	VariableSymbol* symbol = new VariableSymbol(target, node, _enclosingFunction);

	if (node->typeName())
	{
		std::shared_ptr<Type> type = typeTable_->nameToType(node->typeName());
		if (!type)
		{
			std::stringstream msg;
			msg << "unknown type \"" << node->typeName()->name() << "\".";
			semanticError(node, msg.str());
			return;
		}

		symbol->type = type;
	}

	topScope()->insert(symbol);
	node->attachSymbol(symbol);
}

void SemanticPass2::visit(MatchNode* node)
{
	AstVisitor::visit(node);

	const std::string& constructor = node->constructor();
	Symbol* symbol = searchScopes(constructor);

	if (symbol == nullptr)
	{
		std::stringstream msg;
		msg << "constructor \"" << constructor << "\" is not defined.";

		semanticError(node, msg.str());
	}

	// A symbol with a capital letter should always be a constructor
	assert(symbol->kind == kFunction);

	FunctionSymbol* constructorSymbol = static_cast<FunctionSymbol*>(symbol);
	assert(constructorSymbol->type->isBoxed());

	if (constructorSymbol->type->valueConstructors().size() != 1)
	{
		std::stringstream msg;
		msg << "let statement pattern matching only applies to types with a single constructor.";
		semanticError(node, msg.str());
	}

	if (constructorSymbol->arity != node->params()->names().size())
	{
		std::stringstream msg;
		msg << "constructor pattern \"" << symbol->name << "\" does not have the correct number of arguments.";
		semanticError(node, msg.str());
	}

	node->attachConstructorSymbol(constructorSymbol);

	// And create new variables for each of the members of the constructor
	for (size_t i = 0; i < node->params()->names().size(); ++i)
	{
		const std::string& name = node->params()->names().at(i);

		if (topScope()->find(name) != nullptr)
		{
			std::stringstream msg;
			msg << "symbol \"" << name << "\" has already been defined.";
			semanticError(node, msg.str());
		}

		VariableSymbol* member = new VariableSymbol(name, node, _enclosingFunction);
		member->type = constructorSymbol->paramTypes[i];
		topScope()->insert(member);
		node->attachSymbol(member);
	}
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

void TypeChecker::typeCheck(AstNode* node, const std::shared_ptr<Type>& type)
{
	if (!equals(node->type(), type))
	{
		std::stringstream msg;
		msg << "expected type " << type->name() << ", but got " << node->type()->name();

		semanticError(node, msg.str());
	}
}

// Internal nodes
void TypeChecker::visit(ProgramNode* node)
{
	typeTable_ = node->typeTable();

	for (auto& child : node->children())
	{
		child->accept(this);
		typeCheck(child.get(), typeTable_->getBaseType("Unit"));
	}

	node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(ComparisonNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), typeTable_->getBaseType("Int"));

	node->rhs()->accept(this);
	typeCheck(node->rhs(), typeTable_->getBaseType("Int"));

	node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(BinaryOperatorNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), typeTable_->getBaseType("Int"));

	node->rhs()->accept(this);
	typeCheck(node->rhs(), typeTable_->getBaseType("Int"));

	node->setType(typeTable_->getBaseType("Int"));
}

void TypeChecker::visit(NullNode* node)
{
	node->child()->accept(this);

	// FIXME: This should probably be done in a better way
	if (!node->child()->type()->isBoxed())
	{
		std::stringstream msg;
		msg << "cannot call null on unboxed type " << node->child()->type()->name();
		semanticError(node, msg.str());
	}

	node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(LogicalNode* node)
{
	node->lhs()->accept(this);
	typeCheck(node->lhs(), typeTable_->getBaseType("Bool"));

	node->rhs()->accept(this);
	typeCheck(node->rhs(), typeTable_->getBaseType("Bool"));

	node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(MatchNode* node)
{
	node->body()->accept(this);
	typeCheck(node->body(), node->constructorSymbol()->type);

	node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(BlockNode* node)
{
	for (auto& child : node->children())
	{
		child->accept(this);
		typeCheck(child.get(), typeTable_->getBaseType("Unit"));
	}

	node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(IfNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), typeTable_->getBaseType("Bool"));

	node->body()->accept(this);

	node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(IfElseNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), typeTable_->getBaseType("Bool"));

	node->body()->accept(this);
	node->else_body()->accept(this);

	node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(WhileNode* node)
{
	node->condition()->accept(this);
	typeCheck(node->condition(), typeTable_->getBaseType("Bool"));

	node->body()->accept(this);

	node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(AssignNode* node)
{
	node->value()->accept(this);
	typeCheck(node->value(), node->symbol()->type);

	node->setType(typeTable_->getBaseType("Unit"));
}

// Leaf nodes
void TypeChecker::visit(NullaryNode* node)
{
	node->setType(node->symbol()->type);
}

void TypeChecker::visit(IntNode* node)
{
	node->setType(typeTable_->getBaseType("Int"));
}

void TypeChecker::visit(BoolNode* node)
{
	node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(NilNode* node)
{
	std::shared_ptr<Type> listOfInts = ConstructedType::create(
		typeTable_->getTypeConstructor("List"),
		{typeTable_->getBaseType("Int")});

	node->setType(listOfInts);
}

void TypeChecker::visit(FunctionCallNode* node)
{
	// The type of a function call is the return type of the function
	node->setType(node->symbol()->type);

	assert(node->symbol()->paramTypes.size() == node->arguments().size());
	for (size_t i = 0; i < node->arguments().size(); ++i)
	{
		AstNode* argument = node->arguments().at(i).get();
		std::shared_ptr<Type> type = node->symbol()->paramTypes.at(i);

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

	node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(FunctionDefNode* node)
{
	enterScope(node->scope());
	_enclosingFunction = node;

	// Recurse
	node->body()->accept(this);

	_enclosingFunction = nullptr;
	exitScope();

	node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(LetNode* node)
{
	node->value()->accept(this);

	// Baby type inference
	if (!node->typeName())
	{
		node->symbol()->type = node->value()->type();
	}
	else
	{
		typeCheck(node->value(), node->symbol()->type);
	}

	node->setType(typeTable_->getBaseType("Unit"));
}
