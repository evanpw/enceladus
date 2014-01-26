#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include "scope.hpp"
#include "semantic.hpp"
#include "simple.tab.h"
#include "utility.hpp"

using namespace std;

SemanticAnalyzer::SemanticAnalyzer(ProgramNode* root)
: root_(root)
, _enclosingFunction(nullptr)
{
}

bool SemanticAnalyzer::analyze()
{
	try
	{
		root_->accept(this);
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}

void SemanticAnalyzer::semanticError(AstNode* node, const std::string& msg)
{
	std::stringstream ss;

	ss << "Near line " << node->location()->first_line << ", "
	   << "column " << node->location()->first_column << ": "
	   << "error: " << msg;

	throw SemanticError(ss.str());
}

void SemanticAnalyzer::injectSymbols(ProgramNode* node)
{
	Scope* scope = node->scope();
	TypeTable* typeTable = node->typeTable();

	const TypeConstructor* List = typeTable->getTypeConstructor("List");

	//// Create symbols for built-in functions
	FunctionSymbol* notFn = new FunctionSymbol("not", node, nullptr);
	notFn->typeScheme = TypeScheme::trivial(FunctionType::create({TypeTable::Bool}, TypeTable::Bool));
	notFn->isBuiltin = true;
	scope->insert(notFn);

	FunctionSymbol* head = new FunctionSymbol("head", node, nullptr);
	std::shared_ptr<Type> a1 = newVariable();
	std::shared_ptr<Type> headType =
		FunctionType::create(
			{ConstructedType::create(List, {a1})},
			a1);
	head->typeScheme = std::shared_ptr<TypeScheme>(new TypeScheme(headType, {a1->get<TypeVariable>()}));
	head->isBuiltin = true;
	scope->insert(head);

	FunctionSymbol* tail = new FunctionSymbol("tail", node, nullptr);
	std::shared_ptr<Type> a2 = newVariable();
	std::shared_ptr<Type> tailType =
		FunctionType::create(
			{ConstructedType::create(List, {a2})},
			ConstructedType::create(List, {a2}));
	tail->typeScheme = std::shared_ptr<TypeScheme>(new TypeScheme(tailType, {a2->get<TypeVariable>()}));
	tail->isBuiltin = true;
	scope->insert(tail);

	FunctionSymbol* cons = new FunctionSymbol("Cons", node, nullptr);
	std::shared_ptr<Type> a3 = newVariable();
	std::shared_ptr<Type> consType =
		FunctionType::create(
			{a3, ConstructedType::create(List, {a3})},
			ConstructedType::create(List, {a3}));
	cons->typeScheme = std::shared_ptr<TypeScheme>(new TypeScheme(consType, {a3->get<TypeVariable>()}));
	cons->isExternal = true;
	cons->isForeign = true;
	scope->insert(cons);

	FunctionSymbol* nil = new FunctionSymbol("Nil", node, nullptr);
	std::shared_ptr<Type> a4 = newVariable();
	std::shared_ptr<Type> nilType =
		FunctionType::create({}, ConstructedType::create(List, {a4}));
	nil->typeScheme = std::shared_ptr<TypeScheme>(new TypeScheme(nilType, {a4->get<TypeVariable>()}));
	nil->isBuiltin = true;
	scope->insert(nil);

	FunctionSymbol* nullFn = new FunctionSymbol("null", node, nullptr);
	std::shared_ptr<Type> a5 = newVariable();
	std::shared_ptr<Type> nullType =
		FunctionType::create({a5}, TypeTable::Bool);
	nullFn->typeScheme = std::shared_ptr<TypeScheme>(new TypeScheme(nullType, {a5->get<TypeVariable>()}));
	nullFn->isBuiltin = true;
	scope->insert(nullFn);

	// Integer arithmetic functions ////////////////////////////////////////////

	FunctionSymbol* add = new FunctionSymbol("+", node, nullptr);
	add->typeScheme = TypeScheme::trivial(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	add->isBuiltin = true;
	scope->insert(add);

	FunctionSymbol* subtract = new FunctionSymbol("-", node, nullptr);
	subtract->typeScheme = TypeScheme::trivial(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	subtract->isBuiltin = true;
	scope->insert(subtract);

	FunctionSymbol* multiply = new FunctionSymbol("*", node, nullptr);
	multiply->typeScheme = TypeScheme::trivial(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	multiply->isBuiltin = true;
	scope->insert(multiply);

	FunctionSymbol* divide = new FunctionSymbol("/", node, nullptr);
	divide->typeScheme = TypeScheme::trivial(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	divide->isBuiltin = true;
	scope->insert(divide);

	FunctionSymbol* modulus = new FunctionSymbol("%", node, nullptr);
	modulus->typeScheme = TypeScheme::trivial(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	modulus->isBuiltin = true;
	scope->insert(modulus);


	//// These definitions are only needed so that we list them as external
	//// symbols in the output assembly file. They can't be called from
	//// language.
	FunctionSymbol* die = new FunctionSymbol("_die", node, nullptr);
	die->isExternal = true;
	die->isForeign = true;
	scope->insert(die);

	FunctionSymbol* incref = new FunctionSymbol("_incref", node, nullptr);
	incref->isExternal = true;
	incref->isForeign = true;
	scope->insert(incref);

	FunctionSymbol* decref = new FunctionSymbol("_decref", node, nullptr);
	decref->isExternal = true;
	decref->isForeign = true;
	scope->insert(decref);

	FunctionSymbol* decref_no_free = new FunctionSymbol("_decrefNoFree", node, nullptr);
	decref_no_free->isExternal = true;
	decref_no_free->isForeign = true;
	scope->insert(decref_no_free);

	FunctionSymbol* mallocFn = new FunctionSymbol("malloc", node, nullptr);
	mallocFn->isExternal = true;
	mallocFn->isForeign = true;
	scope->insert(mallocFn);
}

std::shared_ptr<Type> SemanticAnalyzer::newVariable()
{
	std::shared_ptr<Type> var = TypeVariable::create();
	_variables[var->get<TypeVariable>()].push_back(var);

	return var;
}

void SemanticAnalyzer::bindVariable(const std::shared_ptr<Type>& variable, const std::shared_ptr<Type>& value, AstNode* node)
{
    assert(variable->tag() == ttVariable);

    // Check to see if the value is actually the same type variable, and don't
    // rebind
    if (value->tag() == ttVariable)
    {
        if (variable->get<TypeVariable>() == value->get<TypeVariable>()) return;
    }

    // Make sure that the variable actually occurs in the type
    if (occurs(variable->get<TypeVariable>(), value))
    {
        std::stringstream ss;
        ss << "variable " << variable->name() << " already occurs in " << value->name();
        inferenceError(node, ss.str());
    }

    // And if these check out, make the substitution
    TypeVariable* oldTypeVar = variable->get<TypeVariable>();

    // Each Type that currently points to _oldTypVar_ now points to _value_
    for (std::shared_ptr<Type>& x : _variables[oldTypeVar])
    {
    	*x = *value;
    }

    // If _value_ points to a type variable, then every type that once pointed
    // to _variable_ now points to _value_
    if (value->tag() == ttVariable)
    {
		for (std::shared_ptr<Type>& x : _variables[oldTypeVar])
	    {
	    	_variables[value->get<TypeVariable>()].push_back(x);
	    }
    }

    // No Type should now point to _oldTypeVar_
    _variables.erase(oldTypeVar);
}

void SemanticAnalyzer::inferenceError(AstNode* node, const std::string& msg)
{
    std::stringstream ss;

    ss << "Near line " << node->location()->first_line << ", "
       << "column " << node->location()->first_column << ": "
       << "error: " << msg;

    throw TypeInferenceError(ss.str());
}

std::set<TypeVariable*> SemanticAnalyzer::getFreeVars(Symbol* symbol)
{
    std::set<TypeVariable*> freeVars;

    // FIXME: Some functions (like _die) which cannot be called from in-language
    // have no type scheme because they don't need it. We should have a better
    // way of declaring external symbols without introducing fake in-language
    // functions.
    if (!symbol->typeScheme) return freeVars;

    freeVars += symbol->typeScheme->freeVars();

    if (symbol->kind == kFunction)
    {
        FunctionSymbol* functionSymbol = static_cast<FunctionSymbol*>(symbol);

        assert(functionSymbol->typeScheme->tag() == ttFunction);
        FunctionType* functionType = functionSymbol->typeScheme->type()->get<FunctionType>();
        for (auto& type : functionType->inputs())
        {
            freeVars += type->freeVars();
        }
        freeVars += functionType->output()->freeVars();
    }

    return freeVars;
}

std::unique_ptr<TypeScheme> SemanticAnalyzer::generalize(const std::shared_ptr<Type>& type, const std::vector<Scope*>& scopes)
{
    std::set<TypeVariable*> typeFreeVars = type->freeVars();

    std::set<TypeVariable*> envFreeVars;
    for (auto i = scopes.rbegin(); i != scopes.rend(); ++i)
    {
        Scope* scope = *i;
        for (auto& j : scope->symbols())
        {
            const std::unique_ptr<Symbol>& symbol = j.second;
            envFreeVars += getFreeVars(symbol.get());
        }
    }

    return make_unique<TypeScheme>(type, typeFreeVars - envFreeVars);
}

std::shared_ptr<Type> SemanticAnalyzer::instantiate(const std::shared_ptr<Type>& type, const std::map<TypeVariable*, std::shared_ptr<Type>>& replacements)
{
    switch (type->tag())
    {
        case ttBase:
            return type;

        case ttVariable:
        {
            TypeVariable* typeVariable = type->get<TypeVariable>();

            auto i = replacements.find(typeVariable);
            if (i != replacements.end())
            {
                return i->second;
            }
            else
            {
                return type;
            }
        }

        case ttFunction:
        {
            FunctionType* functionType = type->get<FunctionType>();

            std::vector<std::shared_ptr<Type>> newInputs;
            for (const std::shared_ptr<Type> input : functionType->inputs())
            {
                newInputs.push_back(instantiate(input, replacements));
            }

            return FunctionType::create(newInputs, instantiate(functionType->output(), replacements));
        }

        case ttConstructed:
        {
            std::vector<std::shared_ptr<Type>> params;

            ConstructedType* constructedType = type->get<ConstructedType>();
            for (const std::shared_ptr<Type>& parameter : constructedType->typeParameters())
            {
                params.push_back(instantiate(parameter, replacements));
            }

            return ConstructedType::create(constructedType->typeConstructor(), params);
        }
    }

    assert(false);
    return std::shared_ptr<Type>();
}

std::shared_ptr<Type> SemanticAnalyzer::instantiate(TypeScheme* scheme)
{
    std::map<TypeVariable*, std::shared_ptr<Type>> replacements;
    for (TypeVariable* boundVar : scheme->quantified())
    {
        replacements[boundVar] = newVariable();
    }

    return instantiate(scheme->type(), replacements);
}

bool SemanticAnalyzer::occurs(TypeVariable* variable, const std::shared_ptr<Type>& value)
{
    switch (value->tag())
    {
        case ttBase:
            return false;

        case ttVariable:
        {
            TypeVariable* typeVariable = value->get<TypeVariable>();
            return typeVariable == variable;
        }

        case ttFunction:
        {
            FunctionType* functionType = value->get<FunctionType>();

            for (auto& input : functionType->inputs())
            {
                if (occurs(variable, input)) return true;
            }

            return occurs(variable, functionType->output());
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = value->get<ConstructedType>();
            for (const std::shared_ptr<Type>& parameter : constructedType->typeParameters())
            {
                if (occurs(variable, parameter)) return true;
            }

            return false;
        }
    }

    assert(false);
}

void SemanticAnalyzer::unify(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs, AstNode* node)
{
    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        // Two base types can be unified only if equal (we don't have inheritance)
        if (lhs->name() == rhs->name())
            return;
    }
    else if (lhs->tag() == ttVariable)
    {
        bindVariable(lhs, rhs, node);
        return;
    }
    else if (rhs->tag() == ttVariable)
    {
        bindVariable(rhs, lhs, node);
        return;
    }
    else if (lhs->tag() == ttFunction && rhs->tag() == ttFunction)
    {
        FunctionType* lhsFunction = lhs->get<FunctionType>();
        FunctionType* rhsFunction = rhs->get<FunctionType>();

        if (lhsFunction->inputs().size() == rhsFunction->inputs().size())
        {
            for (size_t i = 0; i < lhsFunction->inputs().size(); ++i)
            {
                unify(lhsFunction->inputs().at(i), rhsFunction->inputs().at(i), node);
            }

            unify(lhsFunction->output(), rhsFunction->output(), node);

            return;
        }
    }
    else if (lhs->tag() == ttConstructed && rhs->tag() == ttConstructed)
    {
        ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
        ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

        if (lhsConstructed->typeConstructor() == rhsConstructed->typeConstructor())
        {
            assert(lhsConstructed->typeParameters().size() == rhsConstructed->typeParameters().size());

            for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
            {
                unify(lhsConstructed->typeParameters().at(i), rhsConstructed->typeParameters().at(i), node);
            }

            return;
        }
    }

    // Can't be unified
    std::stringstream ss;
    ss << "cannot unify types " << lhs->name() << " and " << rhs->name();
    inferenceError(node, ss.str());
}

void SemanticAnalyzer::visit(ProgramNode* node)
{
	injectSymbols(node);

	//// Recurse down into children
	AstVisitor::visit(node);

    for (auto& child : node->children())
    {
        unify(child->type(), TypeTable::Unit, node);
    }

    node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(DataDeclaration* node)
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
	symbol->typeScheme = TypeScheme::trivial(FunctionType::create(memberTypes, newType));
	topScope()->insert(symbol);

	node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(FunctionDefNode* node)
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

	std::vector<std::shared_ptr<Type>> paramTypes;
	std::shared_ptr<Type> returnType;
	if (node->typeDecl())
	{
		// Must have a type specified for each parameter + one for return type
		if (node->typeDecl()->size() != node->params().size() + 1)
		{
			std::stringstream msg;
			msg << "number of types does not match parameter list";
			semanticError(node, msg.str());
			return;
		}

		// Parameter types must be valid
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
		returnType = typeTable_->nameToType(node->typeDecl()->back().get());
		if (!returnType)
		{
			std::stringstream msg;
			msg << "unknown return type \"" << node->typeDecl()->back()->name() << "\".";
			semanticError(node, msg.str());
			return;
		}
	}
	else
	{
		for (size_t i = 0; i < node->params().size(); ++i)
		{
			paramTypes.push_back(newVariable());
		}

		returnType = newVariable();
	}

	// The type scheme of this function is temporarily not generalized. We
	// want to infer whatever concrete types we can within the body of the
	// function.
	FunctionSymbol* symbol = new FunctionSymbol(name, node, node);
	symbol->typeScheme = TypeScheme::trivial(FunctionType::create(paramTypes, returnType));
	topScope()->insert(symbol);
	node->attachSymbol(symbol);

	enterScope(node->scope());

	// Add symbols corresponding to the formal parameters to the
	// function's scope
	for (size_t i = 0; i < node->params().size(); ++i)
	{
		const std::string& param = node->params().at(i);

		VariableSymbol* paramSymbol = new VariableSymbol(param, node, node);
		paramSymbol->isParam = true;
		paramSymbol->typeScheme = TypeScheme::trivial(paramTypes[i]);
		topScope()->insert(paramSymbol);
	}

	// Recurse
	_enclosingFunction = node;
	node->body()->accept(this);
	_enclosingFunction = nullptr;

	exitScope();

	// Once we've looked through the body of the function and inferred whatever
	// types we can, any remaining type variables can be generalized.
	topScope()->release(name);
	symbol->typeScheme = generalize(symbol->typeScheme->type(), scopes_);
	topScope()->insert(symbol);

	node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(ForeignDeclNode* node)
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

	FunctionSymbol* symbol = new FunctionSymbol(name, node, nullptr);
	symbol->typeScheme = TypeScheme::trivial(FunctionType::create(paramTypes, returnType));
	symbol->isForeign = true;
	symbol->isExternal = true;
	topScope()->insert(symbol);
	node->attachSymbol(symbol);

	node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(LetNode* node)
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

		symbol->typeScheme = TypeScheme::trivial(type);
	}
	else
	{
		symbol->typeScheme = TypeScheme::trivial(newVariable());
	}

	topScope()->insert(symbol);
	node->attachSymbol(symbol);

	unify(node->value()->type(), symbol->typeScheme->type(), node);

    node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(MatchNode* node)
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
	assert(constructorSymbol->typeScheme->isBoxed());

	assert(constructorSymbol->typeScheme->tag() == ttFunction);
	FunctionType* functionType = constructorSymbol->typeScheme->type()->get<FunctionType>();
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
		member->typeScheme = TypeScheme::trivial(functionType->inputs().at(i));
		topScope()->insert(member);
		node->attachSymbol(member);
	}

	unify(node->body()->type(), functionType->output(), node);
    node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(AssignNode* node)
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

	node->value()->accept(this);

    assert(node->symbol()->typeScheme->quantified().empty());
    unify(node->value()->type(), node->symbol()->typeScheme->type(), node);

    node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(FunctionCallNode* node)
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

	std::vector<std::shared_ptr<Type>> paramTypes;
    for (size_t i = 0; i < node->arguments().size(); ++i)
    {
        AstNode* argument = node->arguments().at(i).get();
        argument->accept(this);

        paramTypes.push_back(argument->type());
    }

    FunctionSymbol* functionSymbol = static_cast<FunctionSymbol*>(symbol);
	node->attachSymbol(functionSymbol);

    std::shared_ptr<Type> returnType = newVariable();
    std::shared_ptr<Type> functionType = instantiate(functionSymbol->typeScheme.get());

    unify(functionType, FunctionType::create(paramTypes, returnType), node);

    node->setType(returnType);
}

void SemanticAnalyzer::visit(NullaryNode* node)
{
	const std::string& name = node->name();

	Symbol* symbol = searchScopes(name);
	if (symbol != nullptr)
	{
		if (symbol->kind == kVariable)
		{
			node->attachSymbol(symbol);

			assert(symbol->typeScheme->quantified().empty());
        	node->setType(symbol->typeScheme->type());
		}
		else if (symbol->kind == kFunction)
		{
			node->attachSymbol(symbol);

	        std::shared_ptr<Type> returnType = newVariable();
	        std::shared_ptr<Type> functionType = instantiate(symbol->typeScheme.get());
	        unify(functionType, FunctionType::create({}, returnType), node);

	        node->setType(returnType);
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
		msg << "symbol \"" << name << "\" is not defined in this scope.";

		semanticError(node, msg.str());
	}
}

void SemanticAnalyzer::visit(ComparisonNode* node)
{
    node->lhs()->accept(this);
    unify(node->lhs()->type(), TypeTable::Int, node);

    node->rhs()->accept(this);
    unify(node->rhs()->type(), TypeTable::Int, node);

    node->setType(TypeTable::Bool);
}

void SemanticAnalyzer::visit(LogicalNode* node)
{
    node->lhs()->accept(this);
    unify(node->lhs()->type(), TypeTable::Bool, node);

    node->rhs()->accept(this);
    unify(node->rhs()->type(), TypeTable::Bool, node);

    node->setType(TypeTable::Bool);
}

void SemanticAnalyzer::visit(BlockNode* node)
{
    for (auto& child : node->children())
    {
        child->accept(this);
        unify(child->type(), TypeTable::Unit, node);
    }

    node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(IfNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), TypeTable::Bool, node);

    node->body()->accept(this);
    unify(node->body()->type(), TypeTable::Unit, node);

    node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(IfElseNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), TypeTable::Bool, node);

    node->body()->accept(this);
    unify(node->body()->type(), TypeTable::Unit, node);

    node->else_body()->accept(this);
    unify(node->else_body()->type(), TypeTable::Unit, node);

    node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(WhileNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), TypeTable::Bool, node);

    node->body()->accept(this);
    unify(node->body()->type(), TypeTable::Unit, node);

    node->setType(TypeTable::Unit);
}

void SemanticAnalyzer::visit(IntNode* node)
{
    node->setType(TypeTable::Int);
}

void SemanticAnalyzer::visit(BoolNode* node)
{
    node->setType(TypeTable::Bool);
}

void SemanticAnalyzer::visit(ReturnNode* node)
{
    if (_enclosingFunction == nullptr)
    {
        std::stringstream msg;
        msg << "Cannot return from top level.";
        semanticError(node, msg.str());

        return;
    }

    node->expression()->accept(this);

    assert(_enclosingFunction->symbol()->typeScheme->quantified().empty());
    assert(_enclosingFunction->symbol()->typeScheme->tag() == ttFunction);

    // Value of expression must equal the return type of the enclosing function.
    FunctionType* functionType = _enclosingFunction->symbol()->typeScheme->type()->get<FunctionType>();

    unify(node->expression()->type(), functionType->output(), node);

    node->setType(TypeTable::Unit);
}
