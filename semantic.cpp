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
	try
	{
		SemanticPass1 pass1;
		root_->accept(&pass1);

		SemanticPass2 pass2;
		root_->accept(&pass2);

		TypeChecker typeChecker;
		root_->accept(&typeChecker);
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}

void semanticError(AstNode* node, const std::string& msg)
{
	std::stringstream ss;

	ss << "Near line " << node->location()->first_line << ", "
	   << "column " << node->location()->first_column << ": "
	   << "error: " << msg;

	throw SemanticError(ss.str());
}

void SemanticPass1::visit(ProgramNode* node)
{
	Scope* scope = node->scope();
	TypeTable* typeTable = node->typeTable();

	const TypeConstructor* List = typeTable->getTypeConstructor("List");

	//// Create symbols for built-in functions
	FunctionSymbol* notFn = new FunctionSymbol("not", node, nullptr);
	notFn->type = TypeScheme::trivial(FunctionType::create({TypeTable::Bool}, TypeTable::Bool));
	notFn->isBuiltin = true;
	scope->insert(notFn);

	FunctionSymbol* head = new FunctionSymbol("head", node, nullptr);
	std::shared_ptr<Type> a1 = TypeVariable::create();
	std::shared_ptr<Type> headType =
		FunctionType::create(
			{ConstructedType::create(List, {a1})},
			a1);
	head->type = std::shared_ptr<TypeScheme>(new TypeScheme(headType, {a1->get<TypeVariable>()}));
	head->isBuiltin = true;
	scope->insert(head);

	FunctionSymbol* tail = new FunctionSymbol("tail", node, nullptr);
	std::shared_ptr<Type> a2 = TypeVariable::create();
	std::shared_ptr<Type> tailType =
		FunctionType::create(
			{ConstructedType::create(List, {a2})},
			ConstructedType::create(List, {a2}));
	tail->type = std::shared_ptr<TypeScheme>(new TypeScheme(tailType, {a2->get<TypeVariable>()}));
	tail->isBuiltin = true;
	scope->insert(tail);

	FunctionSymbol* cons = new FunctionSymbol("Cons", node, nullptr);
	std::shared_ptr<Type> a3 = TypeVariable::create();
	std::shared_ptr<Type> consType =
		FunctionType::create(
			{a3, ConstructedType::create(List, {a3})},
			ConstructedType::create(List, {a3}));
	cons->type = std::shared_ptr<TypeScheme>(new TypeScheme(consType, {a3->get<TypeVariable>()}));
	cons->isExternal = true;
	cons->isForeign = true;
	scope->insert(cons);

	FunctionSymbol* nil = new FunctionSymbol("Nil", node, nullptr);
	std::shared_ptr<Type> a4 = TypeVariable::create();
	std::shared_ptr<Type> nilType =
		FunctionType::create({}, ConstructedType::create(List, {a4}));
	nil->type = std::shared_ptr<TypeScheme>(new TypeScheme(nilType, {a4->get<TypeVariable>()}));
	nil->isBuiltin = true;
	scope->insert(nil);

	FunctionSymbol* nullFn = new FunctionSymbol("null", node, nullptr);
	std::shared_ptr<Type> a5 = TypeVariable::create();
	std::shared_ptr<Type> nullType =
		FunctionType::create({a5}, TypeTable::Bool);
	nullFn->type = std::shared_ptr<TypeScheme>(new TypeScheme(nullType, {a5->get<TypeVariable>()}));
	nullFn->isBuiltin = true;
	scope->insert(nullFn);


	//// These definitions are only needed so that we list them as external
	//// symbols in the output assembly file. They can't be called from
	//// language.
	FunctionSymbol* die = new FunctionSymbol("_die", node, nullptr);
	die->isExternal = true;
	scope->insert(die);

	FunctionSymbol* incref = new FunctionSymbol("_incref", node, nullptr);
	incref->isExternal = true;
	scope->insert(incref);

	FunctionSymbol* decref = new FunctionSymbol("_decref", node, nullptr);
	decref->isExternal = true;
	scope->insert(decref);

	FunctionSymbol* decref_no_free = new FunctionSymbol("_decrefNoFree", node, nullptr);
	decref_no_free->isExternal = true;
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
	symbol->type = TypeScheme::trivial(FunctionType::create(memberTypes, newType));
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

	// TODO: Generic functions

	FunctionSymbol* symbol = new FunctionSymbol(name, node, _enclosingFunction);
	symbol->type = TypeScheme::trivial(FunctionType::create(paramTypes, returnType));
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
		paramSymbol->type = TypeScheme::trivial(paramTypes[i]);
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
	symbol->type = TypeScheme::trivial(FunctionType::create(paramTypes, returnType));
	symbol->isForeign = true;
	symbol->isExternal = true;
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

		symbol->type = TypeScheme::trivial(type);
	}
	else
	{
		symbol->type = TypeScheme::trivial(TypeVariable::create());
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

	assert(constructorSymbol->type->tag() == ttFunction);
	FunctionType* functionType = constructorSymbol->type->type()->get<FunctionType>();
	const std::shared_ptr<Type> constructedType = functionType->output();

	if (constructedType->valueConstructors().size() != 1)
	{
		std::stringstream msg;
		msg << "let statement pattern matching only applies to types with a single constructor ";
		semanticError(node, msg.str());
	}

	if (functionType->inputs().size() != node->params()->names().size())
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
		member->type = TypeScheme::trivial(functionType->inputs().at(i));
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
		// TODO: Shouldn't this check be done in type checking?

		FunctionSymbol* functionSymbol = static_cast<FunctionSymbol*>(symbol);
		node->attachSymbol(functionSymbol);

		assert(functionSymbol->type->tag() == ttFunction);
		FunctionType* functionType = functionSymbol->type->type()->get<FunctionType>();
		if (functionType->inputs().size() != node->arguments().size())
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
