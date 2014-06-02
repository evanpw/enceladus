#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <boost/lexical_cast.hpp>

#include "scope.hpp"
#include "semantic.hpp"
#include "parser.hpp"
#include "utility.hpp"


//// Utility functions /////////////////////////////////////////////////////////

#define CHECK(p, ...) if (!(p)) { semanticError(node, __VA_ARGS__); }

template<typename... Args>
void SemanticAnalyzer::semanticError(AstNode* node, const std::string& str, Args... args)
{
    std::stringstream ss;

    ss << "Near line " << node->location->first_line << ", "
       << "column " << node->location->first_column << ": "
       << "error: " << format(str, args...);

    throw SemanticError(ss.str());
}

Symbol* SemanticAnalyzer::searchScopes(const std::string& name)
{
    for (auto i = _scopes.rbegin(); i != _scopes.rend(); ++i)
    {
        Symbol* symbol = (*i)->find(name);
        if (symbol != nullptr) return symbol;
    }

    return nullptr;
}

SemanticAnalyzer::SemanticAnalyzer(ProgramNode* root)
: _root(root)
, _enclosingFunction(nullptr)
{
}

bool SemanticAnalyzer::analyze()
{
	try
	{
		_root->accept(this);
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}

void SemanticAnalyzer::injectSymbols(ProgramNode* node)
{
    std::shared_ptr<Scope>& scope = node->scope;

	const TypeConstructor* List = _typeTable.getTypeConstructor("List");

	//// Create symbols for built-in functions
    Symbol* notFn = makeFunctionSymbol("not", node, nullptr);
	notFn->setType(FunctionType::create({TypeTable::Bool}, TypeTable::Bool));
	notFn->asFunction.isBuiltin = true;
	scope->insert(notFn);

	Symbol* head = makeFunctionSymbol("head", node, nullptr);
	std::shared_ptr<Type> a1 = newVariable();
	std::shared_ptr<Type> headType =
		FunctionType::create(
			{ConstructedType::create(List, {a1})},
			a1);
	head->setTypeScheme(std::shared_ptr<TypeScheme>(new TypeScheme(headType, {a1->get<TypeVariable>()})));
	head->asFunction.isBuiltin = true;
	scope->insert(head);

	Symbol* tail = makeFunctionSymbol("tail", node, nullptr);
	std::shared_ptr<Type> a2 = newVariable();
	std::shared_ptr<Type> tailType =
		FunctionType::create(
			{ConstructedType::create(List, {a2})},
			ConstructedType::create(List, {a2}));
	tail->setTypeScheme(std::shared_ptr<TypeScheme>(new TypeScheme(tailType, {a2->get<TypeVariable>()})));
	tail->asFunction.isBuiltin = true;
	scope->insert(tail);

	Symbol* cons = makeFunctionSymbol("Cons", node, nullptr);
	std::shared_ptr<Type> a3 = newVariable();
	std::shared_ptr<Type> consType =
		FunctionType::create(
			{a3, ConstructedType::create(List, {a3})},
			ConstructedType::create(List, {a3}));
	cons->setTypeScheme(std::shared_ptr<TypeScheme>(new TypeScheme(consType, {a3->get<TypeVariable>()})));
	cons->asFunction.isExternal = true;
	cons->asFunction.isForeign = true;
	scope->insert(cons);

	Symbol* nil = makeFunctionSymbol("Nil", node, nullptr);
	std::shared_ptr<Type> a4 = newVariable();
	std::shared_ptr<Type> nilType =
		FunctionType::create({}, ConstructedType::create(List, {a4}));
	nil->setTypeScheme(std::shared_ptr<TypeScheme>(new TypeScheme(nilType, {a4->get<TypeVariable>()})));
	nil->asFunction.isBuiltin = true;
	scope->insert(nil);

	Symbol* nullFn = makeFunctionSymbol("null", node, nullptr);
	std::shared_ptr<Type> a5 = newVariable();
	std::shared_ptr<Type> nullType =
		FunctionType::create({a5}, TypeTable::Bool);
	nullFn->setTypeScheme(std::shared_ptr<TypeScheme>(new TypeScheme(nullType, {a5->get<TypeVariable>()})));
	nullFn->asFunction.isBuiltin = true;
	scope->insert(nullFn);

	// Integer arithmetic functions ////////////////////////////////////////////

	Symbol* add = makeFunctionSymbol("+", node, nullptr);
	add->setType(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	add->asFunction.isBuiltin = true;
	scope->insert(add);

	Symbol* subtract = makeFunctionSymbol("-", node, nullptr);
	subtract->setType(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	subtract->asFunction.isBuiltin = true;
	scope->insert(subtract);

	Symbol* multiply = makeFunctionSymbol("*", node, nullptr);
	multiply->setType(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	multiply->asFunction.isBuiltin = true;
	scope->insert(multiply);

	Symbol* divide = makeFunctionSymbol("/", node, nullptr);
	divide->setType(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	divide->asFunction.isBuiltin = true;
	scope->insert(divide);

	Symbol* modulus = makeFunctionSymbol("%", node, nullptr);
	modulus->setType(FunctionType::create({TypeTable::Int, TypeTable::Int}, TypeTable::Int));
	modulus->asFunction.isBuiltin = true;
	scope->insert(modulus);


	//// These definitions are only needed so that we list them as external
	//// symbols in the output assembly file. They can't be called from
	//// language.
	Symbol* die = makeFunctionSymbol("_die", node, nullptr);
	die->asFunction.isExternal = true;
	die->asFunction.isForeign = true;
	scope->insert(die);

	Symbol* incref = makeFunctionSymbol("_incref", node, nullptr);
	incref->asFunction.isExternal = true;
	incref->asFunction.isForeign = true;
	scope->insert(incref);

	Symbol* decref = makeFunctionSymbol("_decref", node, nullptr);
	decref->asFunction.isExternal = true;
	decref->asFunction.isForeign = true;
	scope->insert(decref);

	Symbol* decref_no_free = makeFunctionSymbol("_decrefNoFree", node, nullptr);
	decref_no_free->asFunction.isExternal = true;
	decref_no_free->asFunction.isForeign = true;
	scope->insert(decref_no_free);

	Symbol* mallocFn = makeFunctionSymbol("malloc", node, nullptr);
	mallocFn->asFunction.isExternal = true;
	mallocFn->asFunction.isForeign = true;
	scope->insert(mallocFn);
}


//// Type inference functions //////////////////////////////////////////////////

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

    ss << "Near line " << node->location->first_line << ", "
       << "column " << node->location->first_column << ": "
       << "error: " << msg;

    throw TypeInferenceError(ss.str());
}

std::set<TypeVariable*> SemanticAnalyzer::getFreeVars(Symbol& symbol)
{
    std::set<TypeVariable*> freeVars;

    // FIXME: Some functions (like _die) which cannot be called from in-language
    // have no type scheme because they don't need it. We should have a better
    // way of declaring external symbols without introducing fake in-language
    // functions.
    if (!symbol.typeScheme) return freeVars;

    freeVars += symbol.typeScheme->freeVars();

    if (symbol.kind == kFunction)
    {
        assert(symbol.typeScheme->tag() == ttFunction);
        FunctionType* functionType = symbol.typeScheme->type()->get<FunctionType>();
        for (auto& type : functionType->inputs())
        {
            freeVars += type->freeVars();
        }
        freeVars += functionType->output()->freeVars();
    }

    return freeVars;
}

std::unique_ptr<TypeScheme> SemanticAnalyzer::generalize(const std::shared_ptr<Type>& type, const std::vector<std::shared_ptr<Scope>>& scopes)
{
    std::set<TypeVariable*> typeFreeVars = type->freeVars();

    std::set<TypeVariable*> envFreeVars;
    for (auto i = scopes.rbegin(); i != scopes.rend(); ++i)
    {
        const std::shared_ptr<Scope>& scope = *i;
        for (auto& j : scope->symbols)
        {
            const std::unique_ptr<Symbol>& symbol = j.second;
            envFreeVars += getFreeVars(*symbol);
        }
    }

    return make_unique<TypeScheme>(type, typeFreeVars - envFreeVars);
}

std::shared_ptr<Type> SemanticAnalyzer::instantiate(const std::shared_ptr<Type>& type, const std::map<TypeVariable*, std::shared_ptr<Type>>& replacements)
{
    switch (type->tag())
    {
        case ttBase:
        case ttStruct:
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
        case ttStruct:
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
    assert(lhs && rhs && node);

    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        // Two base types can be unified only if equal (we don't have inheritance)
        if (lhs->name() == rhs->name())
            return;
    }
    else if (lhs->tag() == ttStruct && rhs->tag() == ttStruct)
    {
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


//// Visitor functions /////////////////////////////////////////////////////////

void SemanticAnalyzer::visit(ProgramNode* node)
{
    enterScope(node->scope);

	injectSymbols(node);

	//// Recurse down into children
	AstVisitor::visit(node);

    for (auto& child : node->children)
    {
        unify(child->type, TypeTable::Unit, node);
    }

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(DataDeclaration* node)
{
	// Data declarations cannot be local
    CHECK(!_enclosingFunction, "data declarations must be at top level");

	// The data type and constructor name cannot have already been used for something
	const std::string& typeName = node->name;
    CHECK(!searchScopes(typeName), "symbol \"{}\" is already defined", typeName);
    CHECK(!_typeTable.getBaseType(typeName), "symbol \"{}\" is already defined", typeName);
    CHECK(!_typeTable.getTypeConstructor(typeName), "symbol \"{}\" is already defined", typeName);

	const std::string& constructorName = node->constructor->name;
    CHECK(!searchScopes(constructorName), "symbol \"{}\" is already defined", constructorName);
    CHECK(!_typeTable.getBaseType(constructorName), "symbol \"{}\" is already defined", constructorName);
    CHECK(!_typeTable.getTypeConstructor(constructorName), "symbol \"{}\" is already defined", constructorName);

	// All of the constructor members must refer to already-declared types
	std::vector<std::shared_ptr<Type>> memberTypes;
	for (auto& memberTypeName : node->constructor->members())
	{
		std::shared_ptr<Type> memberType = _typeTable.nameToType(*memberTypeName);
        CHECK(memberType, "unknown member type \"{}\"", memberTypeName->str());

		memberTypes.push_back(memberType);
	}
	node->constructor->setMemberTypes(memberTypes);

	// Actually create the type
	std::shared_ptr<Type> newType = BaseType::create(node->name);
	_typeTable.insert(typeName, newType);

	ValueConstructor* valueConstructor = new ValueConstructor(node->constructor->name, memberTypes);
	node->valueConstructor = valueConstructor;
	newType->addValueConstructor(valueConstructor);

	// Create a symbol for the constructor
	Symbol* symbol = makeFunctionSymbol(constructorName, node, nullptr);
	symbol->setType(FunctionType::create(memberTypes, newType));
	topScope()->insert(symbol);

	node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(TypeAliasNode* node)
{
    // Type aliases cannot be local
    CHECK(!_enclosingFunction, "type alias declarations must be at top level");

    // The new type name cannot have already been used
    const std::string& typeName = node->name;
    CHECK(!searchScopes(typeName), "symbol \"{}\" is already defined", typeName);
    CHECK(!_typeTable.getBaseType(typeName), "symbol \"{}\" is already defined", typeName);
    CHECK(!_typeTable.getTypeConstructor(typeName), "symbol \"{}\" is already defined", typeName);

    std::shared_ptr<Type> underlying = _typeTable.nameToType(*node->underlying);
    CHECK(underlying, "unknown type \"{}\"", node->underlying->name());

    // Insert the alias into the type table
    _typeTable.insert(typeName, underlying);

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(FunctionDefNode* node)
{
	// Functions cannot be declared inside of another function
    CHECK(!_enclosingFunction, "functions cannot be nested");

	// The function name cannot have already been used as something else
	const std::string& name = node->name;
    CHECK(!searchScopes(name), "symbol \"{}\" is already defined", name);
    CHECK(!_typeTable.getBaseType(name), "symbol \"{}\" is already defined", name);
    CHECK(!_typeTable.getTypeConstructor(name), "symbol \"{}\" is already defined", name);

	std::vector<std::shared_ptr<Type>> paramTypes;
	std::shared_ptr<Type> returnType;
	if (node->typeDecl)
	{
		// Must have a type specified for each parameter + one for return type
        CHECK(node->typeDecl->size() == node->params->size() + 1, "number of types does not match parameter list");

		// Parameter types must be valid
		for (size_t i = 0; i < node->typeDecl->size() - 1; ++i)
		{
			auto& typeName = node->typeDecl->at(i);

			std::shared_ptr<Type> type = _typeTable.nameToType(*typeName);
            CHECK(type, "unknown parameter type \"{}\"", typeName->str());

			paramTypes.push_back(type);
		}

		// Return type must be valid
		returnType = _typeTable.nameToType(*node->typeDecl->back());
        CHECK(returnType, "unknown return type \"{}\"", node->typeDecl->back()->name());
	}
	else
	{
		for (size_t i = 0; i < node->params->size(); ++i)
		{
			paramTypes.push_back(newVariable());
		}

		returnType = newVariable();
	}

	// The type scheme of this function is temporarily not generalized. We
	// want to infer whatever concrete types we can within the body of the
	// function.
	Symbol* symbol = makeFunctionSymbol(name, node, node);
	symbol->setType(FunctionType::create(paramTypes, returnType));
	topScope()->insert(symbol);
	node->symbol = symbol;

	enterScope(node->scope);

	// Add symbols corresponding to the formal parameters to the
	// function's scope
	for (size_t i = 0; i < node->params->size(); ++i)
	{
		const std::string& param = node->params->at(i);

		Symbol* paramSymbol = makeVariableSymbol(param, node, node);
		paramSymbol->asVariable.isParam = true;
		paramSymbol->setType(paramTypes[i]);
		topScope()->insert(paramSymbol);
	}

	// Recurse
	_enclosingFunction = node;
	node->body->accept(this);
	_enclosingFunction = nullptr;

	exitScope();

	// Once we've looked through the body of the function and inferred whatever
	// types we can, any remaining type variables can be generalized.
	topScope()->release(name);
	symbol->setTypeScheme(generalize(symbol->typeScheme->type(), _scopes));
	topScope()->insert(symbol);

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(ForeignDeclNode* node)
{
	// Functions cannot be declared inside of another function
    CHECK(!_enclosingFunction, "foreign function declarations must be at top level");

	// The function name cannot have already been used as something else
	const std::string& name = node->name;
    CHECK(!searchScopes(name), "symbol \"{}\" is already defined", name);
    CHECK(!_typeTable.getBaseType(name), "symbol \"{}\" is already defined", name);
    CHECK(!_typeTable.getTypeConstructor(name), "symbol \"{}\" is already defined", name);

	// If parameters names are given, must have a type specified for
	// each parameter + one for return type
    CHECK(node->params->size() == 0 || node->typeDecl->size() == node->params->size() + 1, "number of types does not match parameter list");

	// We currently only support 6 function arguments for foreign functions
	// (so that we only have to pass arguments in registers)
    CHECK(node->params->size() <= 6, "a maximum of 6 arguments is supported for foreign functions");

	// Parameter types must be valid
	std::vector<std::shared_ptr<Type>> paramTypes;
	for (size_t i = 0; i < node->typeDecl->size() - 1; ++i)
	{
		auto& typeName = node->typeDecl->at(i);

		std::shared_ptr<Type> type = _typeTable.nameToType(*typeName);
        CHECK(type, "unknown parameter type \"{}\"", typeName->str());

		paramTypes.push_back(type);
	}

	// Return type must be valid
	std::shared_ptr<Type> returnType = _typeTable.nameToType(*node->typeDecl->back());
    CHECK(returnType, "unknown return type \"{}\"", node->typeDecl->back()->name());

	Symbol* symbol = makeFunctionSymbol(name, node, nullptr);
	symbol->setType(FunctionType::create(paramTypes, returnType));
	symbol->asFunction.isForeign = true;
	symbol->asFunction.isExternal = true;
	topScope()->insert(symbol);
	node->symbol = symbol;

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(LetNode* node)
{
	// Visit children. Do this first so that we can't have recursive definitions.
	AstVisitor::visit(node);

	const std::string& target = node->target;
	CHECK(!topScope()->find(target), "symbol \"{}\" already declared in this scope", target);

	Symbol* symbol = makeVariableSymbol(target, node, _enclosingFunction);

	if (node->typeName)
	{
		std::shared_ptr<Type> type = _typeTable.nameToType(*node->typeName);
        CHECK(type, "unknown type \"{}\"", node->typeName->str());

		symbol->setType(type);
	}
	else
	{
		symbol->setType(newVariable());
	}

	topScope()->insert(symbol);
	node->symbol = symbol;

	unify(node->value->type, symbol->type, node);

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(MatchNode* node)
{
	AstVisitor::visit(node);

	const std::string& constructor = node->constructor;
	Symbol* constructorSymbol = searchScopes(constructor);
    CHECK(constructorSymbol, "constructor \"{}\" is not defined", constructor);

	// A symbol with a capital letter should always be a constructor
	assert(constructorSymbol->kind == kFunction);

	assert(constructorSymbol->typeScheme->tag() == ttFunction);
	FunctionType* functionType = constructorSymbol->type->get<FunctionType>();
	const std::shared_ptr<Type> constructedType = functionType->output();

    CHECK(constructedType->valueConstructors().size() == 1, "let statement pattern matching only applies to types with a single constructor");
    CHECK(functionType->inputs().size() == node->params->size(), "constructor pattern \"{}\" does not have the correct number of arguments", constructor);

	node->constructorSymbol = constructorSymbol;

	// And create new variables for each of the members of the constructor
	for (size_t i = 0; i < node->params->size(); ++i)
	{
		const std::string& name = node->params->at(i);
        CHECK(!topScope()->find(name), "symbol \"{}\" has already been defined", name);

		Symbol* member = makeVariableSymbol(name, node, _enclosingFunction);
		member->setType(functionType->inputs().at(i));
		topScope()->insert(member);
		node->attachSymbol(member);
	}

	unify(node->body->type, functionType->output(), node);
    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(AssignNode* node)
{
    node->target->accept(this);
	node->value->accept(this);

    unify(node->value->type, node->target->type, node);

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(FunctionCallNode* node)
{
	const std::string& name = node->target;
	Symbol* symbol = searchScopes(name);

    CHECK(symbol, "function \"{}\" is not defined", name);
    CHECK(symbol->kind == kFunction, "target of function call \"{}\" is not a function", symbol->name);

	std::vector<std::shared_ptr<Type>> paramTypes;
    for (size_t i = 0; i < node->arguments->size(); ++i)
    {
        AstNode& argument = *node->arguments->at(i);
        argument.accept(this);

        paramTypes.push_back(argument.type);
    }

	node->symbol = symbol;

    std::shared_ptr<Type> returnType = newVariable();
    std::shared_ptr<Type> functionType = instantiate(symbol->typeScheme.get());

    unify(functionType, FunctionType::create(paramTypes, returnType), node);

    node->type = returnType;
}

void SemanticAnalyzer::visit(NullaryNode* node)
{
	const std::string& name = node->name;

	Symbol* symbol = searchScopes(name);
    CHECK(symbol, "symbol \"{}\" is not defined in this scope", name);
    CHECK(symbol->kind == kVariable || symbol->kind == kFunction, "symbol \"{}\" is not a variable or a function", name);

	if (symbol->kind == kVariable)
	{
		node->symbol = symbol;
        node->type = symbol->type;
	}
	else /* symbol->kind == kFunction */
	{
		node->symbol = symbol;

        std::shared_ptr<Type> returnType = newVariable();
        std::shared_ptr<Type> functionType = instantiate(symbol->typeScheme.get());
        unify(functionType, FunctionType::create({}, returnType), node);

        node->type = returnType;
	}
}

void SemanticAnalyzer::visit(ComparisonNode* node)
{
    node->lhs->accept(this);
    unify(node->lhs->type, TypeTable::Int, node);

    node->rhs->accept(this);
    unify(node->rhs->type, TypeTable::Int, node);

    node->type = TypeTable::Bool;
}

void SemanticAnalyzer::visit(LogicalNode* node)
{
    node->lhs->accept(this);
    unify(node->lhs->type, TypeTable::Bool, node);

    node->rhs->accept(this);
    unify(node->rhs->type, TypeTable::Bool, node);

    node->type = TypeTable::Bool;
}

void SemanticAnalyzer::visit(BlockNode* node)
{
    for (auto& child : node->children)
    {
        child->accept(this);
        unify(child->type, TypeTable::Unit, node);
    }

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(IfNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, TypeTable::Bool, node);

    node->body->accept(this);
    unify(node->body->type, TypeTable::Unit, node);

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(IfElseNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, TypeTable::Bool, node);

    node->body->accept(this);
    unify(node->body->type, TypeTable::Unit, node);

    node->else_body->accept(this);
    unify(node->else_body->type, TypeTable::Unit, node);

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(WhileNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, TypeTable::Bool, node);

    node->body->accept(this);
    unify(node->body->type, TypeTable::Unit, node);

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(IntNode* node)
{
    node->type = TypeTable::Int;
}

void SemanticAnalyzer::visit(BoolNode* node)
{
    node->type = TypeTable::Bool;
}

void SemanticAnalyzer::visit(ReturnNode* node)
{
    CHECK(_enclosingFunction, "Cannot return from top level");

    node->expression->accept(this);

    assert(_enclosingFunction->symbol->type->tag() == ttFunction);

    // Value of expression must equal the return type of the enclosing function.
    FunctionType* functionType = _enclosingFunction->symbol->type->get<FunctionType>();

    unify(node->expression->type, functionType->output(), node);

    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(VariableNode* node)
{
    Symbol* symbol = searchScopes(node->name);
    CHECK(symbol != nullptr, "symbol \"{}\" is not defined in this scope", node->name);
    CHECK(symbol->kind == kVariable, "symbol \"{}\" is not a variable", node->name);

    node->symbol = symbol;
    node->type = symbol->type;
}

void SemanticAnalyzer::visit(StructDefNode* node)
{
    AstVisitor::visit(node);

    // Data declarations cannot be local
    CHECK(!_enclosingFunction, "struct declaration must be at top level");

    // The struct name cannot have already been used for something
    const std::string& structName = node->name;
    CHECK(!searchScopes(structName), "symbol \"{}\" is already defined", structName);
    CHECK(!_typeTable.getBaseType(structName), "symbol \"{}\" is already defined", structName);
    CHECK(!_typeTable.getTypeConstructor(structName), "symbol \"{}\" is already defined", structName);

    // Actually create the type
    std::shared_ptr<Type> newType = StructType::create(structName, node);
    _typeTable.insert(structName, newType);

    node->structType = newType;
    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(MemberDefNode* node)
{
    // All of the constructor members must refer to already-declared types
    std::shared_ptr<Type> type = _typeTable.nameToType(*node->typeName);
    CHECK(type, "unknown member type \"{}\"", node->typeName->str());

    node->memberType = type;
    node->type = TypeTable::Unit;
}

void SemanticAnalyzer::visit(StructInitNode* node)
{
    std::shared_ptr<Type> type = _typeTable.getBaseType(node->structName);
    CHECK(type, "unknown type \"{}\"", node->structName);
    CHECK(type->get<StructType>(), "cannot initialize non-struct type \"{}\"", node->structName);

    node->type = type;
}

void SemanticAnalyzer::visit(MemberAccessNode* node)
{
    const std::string& name = node->varName;

    Symbol* symbol = searchScopes(name);
    CHECK(symbol, "symbol \"{}\" is not defined in this scope", name);
    CHECK(symbol->kind == kVariable, "expected variable but got function name \"{}\"", name);

    node->symbol = symbol;

    std::shared_ptr<Type> type = symbol->type;

    StructType* structType = type->get<StructType>();
    CHECK(structType, "cannot access member of non-struct variable \"{}\"", name);

    auto i = structType->members().find(node->memberName);
    CHECK(i != structType->members().end(), "no such member \"{}\" of variable \"{}\"", node->memberName, name);

    node->memberLocation = i->second.location;
    node->type = i->second.type;
}
