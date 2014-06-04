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

// TODO: Get rid of this; always report a location
#define CHECK_NO_NODE(p, ...) if (!(p)) { semanticErrorNoNode(__VA_ARGS__); }

#define CHECK_UNDEFINED(name) \
    CHECK(!resolveSymbol(name), "symbol \"{}\" is already defined", name); \
    CHECK(!resolveTypeSymbol(name), "symbol \"{}\" is already defined", name)

#define CHECK_UNDEFINED_IN_SCOPE(name) \
    CHECK(!topScope()->symbols.find(name), "symbol \"{}\" is already defined in this scope", name); \
    CHECK(!topScope()->types.find(name), "symbol \"{}\" is already defined in this scope", name)

#define CHECK_TOP_LEVEL(name) \
    CHECK(!_enclosingFunction, "{} must be at top level", name)

template<typename... Args>
void SemanticAnalyzer::semanticError(AstNode* node, const std::string& str, Args... args)
{
    std::stringstream ss;

    ss << "Near line " << node->location->first_line << ", "
       << "column " << node->location->first_column << ": "
       << "error: " << format(str, args...);

    throw SemanticError(ss.str());
}

template<typename... Args>
void SemanticAnalyzer::semanticErrorNoNode(const std::string& str, Args... args)
{
    std::stringstream ss;

    ss << "error: " << format(str, args...);

    throw SemanticError(ss.str());
}

Symbol* SemanticAnalyzer::resolveSymbol(const std::string& name)
{
    for (auto i = _scopes.rbegin(); i != _scopes.rend(); ++i)
    {
        Symbol* symbol = (*i)->symbols.find(name);
        if (symbol != nullptr) return symbol;
    }

    return nullptr;
}

Symbol* SemanticAnalyzer::resolveTypeSymbol(const std::string& name)
{
    for (auto i = _scopes.rbegin(); i != _scopes.rend(); ++i)
    {
        Symbol* symbol = (*i)->types.find(name);
        if (symbol != nullptr) return symbol;
    }

    return nullptr;
}

SemanticAnalyzer::SemanticAnalyzer(ProgramNode* root)
: _root(root)
, _enclosingFunction(nullptr)
, _enclosingLoop(nullptr)
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

FunctionSymbol* SemanticAnalyzer::makeBuiltin(const std::string& name)
{
    FunctionSymbol* symbol = new FunctionSymbol(name, _root, nullptr);
    symbol->isBuiltin = true;

    return symbol;
}

FunctionSymbol* SemanticAnalyzer::makeExternal(const std::string& name)
{
    FunctionSymbol* symbol = new FunctionSymbol(name, _root, nullptr);
    symbol->isExternal = true;
    symbol->isForeign = true;

    return symbol;
}

void SemanticAnalyzer::injectSymbols()
{
    std::shared_ptr<Scope>& scope = _root->scope;


    //// Built-in types ////////////////////////////////////////////////////////
    Int = BaseType::create("Int", true);
    Bool = BaseType::create("Bool", true);
    Unit = BaseType::create("Unit", true);

    scope->types.insert(new TypeSymbol("Int", _root, Int));
    scope->types.insert(new TypeSymbol("Bool", _root, Bool));
    scope->types.insert(new TypeSymbol("Unit", _root, Unit));
    scope->types.insert(new TypeSymbol("Tree", _root, BaseType::create("Tree", false)));

    TypeConstructor* List = new TypeConstructor("List", 1);
    scope->types.insert(new TypeConstructorSymbol("List", _root, List));

	//// Create symbols for built-in functions
    FunctionSymbol* notFn = makeBuiltin("not");
	notFn->setType(FunctionType::create({Bool}, Bool));
	scope->symbols.insert(notFn);

	FunctionSymbol* head = makeBuiltin("head");
	std::shared_ptr<Type> a1 = newVariable();
	std::shared_ptr<Type> headType =
		FunctionType::create(
			{ConstructedType::create(List, {a1})},
			a1);
	head->setTypeScheme(TypeScheme::make(headType, {a1->get<TypeVariable>()}));
	scope->symbols.insert(head);

	FunctionSymbol* tail = makeBuiltin("tail");
	std::shared_ptr<Type> a2 = newVariable();
	std::shared_ptr<Type> tailType =
		FunctionType::create(
			{ConstructedType::create(List, {a2})},
			ConstructedType::create(List, {a2}));
	tail->setTypeScheme(TypeScheme::make(tailType, {a2->get<TypeVariable>()}));
	scope->symbols.insert(tail);

	FunctionSymbol* cons = new FunctionSymbol("Cons", _root, nullptr);
	std::shared_ptr<Type> a3 = newVariable();
	std::shared_ptr<Type> consType =
		FunctionType::create(
			{a3, ConstructedType::create(List, {a3})},
			ConstructedType::create(List, {a3}));
	cons->setTypeScheme(TypeScheme::make(consType, {a3->get<TypeVariable>()}));
	cons->isExternal = true;
	cons->isForeign = true;
	scope->symbols.insert(cons);

	FunctionSymbol* nil = makeBuiltin("Nil");
	std::shared_ptr<Type> a4 = newVariable();
	std::shared_ptr<Type> nilType =
		FunctionType::create({}, ConstructedType::create(List, {a4}));
	nil->setTypeScheme(TypeScheme::make(nilType, {a4->get<TypeVariable>()}));
	scope->symbols.insert(nil);

	FunctionSymbol* nullFn = makeBuiltin("null");
	std::shared_ptr<Type> a5 = newVariable();
	std::shared_ptr<Type> nullType =
		FunctionType::create({a5}, Bool);
	nullFn->setTypeScheme(TypeScheme::make(nullType, {a5->get<TypeVariable>()}));
	scope->symbols.insert(nullFn);

	//// Integer arithmetic functions //////////////////////////////////////////

    std::shared_ptr<Type> arithmeticType = FunctionType::create({Int, Int}, Int);

	FunctionSymbol* add = makeBuiltin("+");
	add->setType(arithmeticType);
	scope->symbols.insert(add);

	FunctionSymbol* subtract = makeBuiltin("-");
	subtract->setType(arithmeticType);
	scope->symbols.insert(subtract);

	FunctionSymbol* multiply = makeBuiltin("*");
	multiply->setType(arithmeticType);
	scope->symbols.insert(multiply);

	FunctionSymbol* divide = makeBuiltin("/");
	divide->setType(arithmeticType);
	scope->symbols.insert(divide);

	FunctionSymbol* modulus = makeBuiltin("%");
	modulus->setType(arithmeticType);
	scope->symbols.insert(modulus);


	//// These definitions are only needed so that we list them as external
	//// symbols in the output assembly file. They can't be called from
	//// language.
    scope->symbols.insert(makeExternal("_die"));
    scope->symbols.insert(makeExternal("_incref"));
    scope->symbols.insert(makeExternal("_decref"));
    scope->symbols.insert(makeExternal("_decrefNoFree"));
    scope->symbols.insert(makeExternal("malloc"));
}

std::shared_ptr<Type> SemanticAnalyzer::getBaseType(const std::string& name)
{
    Symbol* symbol = resolveTypeSymbol(name);
    CHECK_NO_NODE(symbol, "Base type \"{}\" is not defined", name);
    CHECK_NO_NODE(symbol->kind == kType, "Symbol \"{}\" is not a base type", name);

    return symbol->type;
}

TypeConstructor* SemanticAnalyzer::getTypeConstructor(const std::string& name)
{
    Symbol* symbol = resolveTypeSymbol(name);
    CHECK_NO_NODE(symbol, "Type constructor \"{}\" is not defined", name);
    CHECK_NO_NODE(symbol->kind == kTypeConstructor, "Symbol \"{}\" is not a type constructor", name);

    return symbol->asTypeConstructor()->typeConstructor.get();
}

std::shared_ptr<Type> SemanticAnalyzer::resolveTypeName(const TypeName& typeName)
{
    if (typeName.parameters().empty())
    {
        return getBaseType(typeName.name());
    }
    else
    {
        const TypeConstructor* typeConstructor = getTypeConstructor(typeName.name());

        std::vector<std::shared_ptr<Type>> typeParameters;
        for (auto& parameter : typeName.parameters())
        {
            typeParameters.push_back(resolveTypeName(*parameter));
        }

        return ConstructedType::create(typeConstructor, typeParameters);
    }
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
        for (auto& j : scope->symbols.symbols)
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
	injectSymbols();

	//// Recurse down into children
	AstVisitor::visit(node);

    for (auto& child : node->children)
    {
        unify(child->type, Unit, node);
    }

    exitScope();

    node->type = Unit;
}

void SemanticAnalyzer::visit(DataDeclaration* node)
{
	// Data declarations cannot be local
    CHECK_TOP_LEVEL("data declaration");

	// The data type and constructor name cannot have already been used for something
	const std::string& typeName = node->name;
    CHECK_UNDEFINED(typeName);

	const std::string& constructorName = node->constructor->name;
    CHECK_UNDEFINED(constructorName);

	// All of the constructor members must refer to already-declared types
	std::vector<std::shared_ptr<Type>> memberTypes;
	for (auto& memberTypeName : node->constructor->members())
	{
		std::shared_ptr<Type> memberType = resolveTypeName(*memberTypeName);
        CHECK(memberType, "unknown member type \"{}\"", memberTypeName->str());

		memberTypes.push_back(memberType);
	}
	node->constructor->setMemberTypes(memberTypes);

	// Actually create the type
	std::shared_ptr<Type> newType = BaseType::create(node->name);
    topScope()->types.insert(new TypeSymbol(typeName, node, newType));

	ValueConstructor* valueConstructor = new ValueConstructor(node->constructor->name, memberTypes);
	node->valueConstructor = valueConstructor;
	newType->addValueConstructor(valueConstructor);

	// Create a symbol for the constructor
	Symbol* symbol = new FunctionSymbol(constructorName, node, nullptr);
	symbol->setType(FunctionType::create(memberTypes, newType));
	topScope()->symbols.insert(symbol);

	node->type = Unit;
}

void SemanticAnalyzer::visit(TypeAliasNode* node)
{
    // Type aliases cannot be local
    CHECK_TOP_LEVEL("type alias declaration");

    // The new type name cannot have already been used
    const std::string& typeName = node->name;
    CHECK_UNDEFINED(typeName);

    std::shared_ptr<Type> underlying = resolveTypeName(*node->underlying);
    CHECK(underlying, "unknown type \"{}\"", node->underlying->name());

    // Insert the alias into the type table
    topScope()->types.insert(new TypeSymbol(typeName, node, underlying));

    node->type = Unit;
}

void SemanticAnalyzer::visit(FunctionDefNode* node)
{
	// Functions cannot be declared inside of another function
    CHECK(!_enclosingFunction, "functions cannot be nested");

	// The function name cannot have already been used as something else
	const std::string& name = node->name;
    CHECK_UNDEFINED(name);

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

			std::shared_ptr<Type> type = resolveTypeName(*typeName);
            CHECK(type, "unknown parameter type \"{}\"", typeName->str());

			paramTypes.push_back(type);
		}

		// Return type must be valid
		returnType = resolveTypeName(*node->typeDecl->back());
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
	Symbol* symbol = new FunctionSymbol(name, node, node);
	symbol->setType(FunctionType::create(paramTypes, returnType));
	topScope()->symbols.insert(symbol);
	node->symbol = symbol;

	enterScope(node->scope);

	// Add symbols corresponding to the formal parameters to the
	// function's scope
	for (size_t i = 0; i < node->params->size(); ++i)
	{
		const std::string& param = node->params->at(i);

		Symbol* paramSymbol = new VariableSymbol(param, node, node);
		paramSymbol->asVariable()->isParam = true;
		paramSymbol->setType(paramTypes[i]);
		topScope()->symbols.insert(paramSymbol);
	}

	// Recurse
	_enclosingFunction = node;
	node->body->accept(this);
	_enclosingFunction = nullptr;

	exitScope();

	// Once we've looked through the body of the function and inferred whatever
	// types we can, any remaining type variables can be generalized.
	topScope()->symbols.release(name);
	symbol->setTypeScheme(generalize(symbol->typeScheme->type(), _scopes));
	topScope()->symbols.insert(symbol);

    node->type = Unit;
}

void SemanticAnalyzer::visit(ForeignDeclNode* node)
{
	// Functions cannot be declared inside of another function
    CHECK_TOP_LEVEL("foreign function declaration");

	// The function name cannot have already been used as something else
	const std::string& name = node->name;
    CHECK_UNDEFINED(name);

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

		std::shared_ptr<Type> type = resolveTypeName(*typeName);
        CHECK(type, "unknown parameter type \"{}\"", typeName->str());

		paramTypes.push_back(type);
	}

	// Return type must be valid
	std::shared_ptr<Type> returnType = resolveTypeName(*node->typeDecl->back());
    CHECK(returnType, "unknown return type \"{}\"", node->typeDecl->back()->name());

	FunctionSymbol* symbol = new FunctionSymbol(name, node, nullptr);
	symbol->setType(FunctionType::create(paramTypes, returnType));
	symbol->isForeign = true;
	symbol->isExternal = true;
	topScope()->symbols.insert(symbol);
	node->symbol = symbol;

    node->type = Unit;
}

void SemanticAnalyzer::visit(LetNode* node)
{
	// Visit children. Do this first so that we can't have recursive definitions.
	AstVisitor::visit(node);

	const std::string& target = node->target;
    CHECK_UNDEFINED_IN_SCOPE(target);

	Symbol* symbol = new VariableSymbol(target, node, _enclosingFunction);

	if (node->typeName)
	{
		std::shared_ptr<Type> type = resolveTypeName(*node->typeName);
        CHECK(type, "unknown type \"{}\"", node->typeName->str());

		symbol->setType(type);
	}
	else
	{
		symbol->setType(newVariable());
	}

	topScope()->symbols.insert(symbol);
	node->symbol = symbol;

	unify(node->value->type, symbol->type, node);

    node->type = Unit;
}

void SemanticAnalyzer::visit(MatchNode* node)
{
	AstVisitor::visit(node);

	const std::string& constructor = node->constructor;
	Symbol* constructorSymbol = resolveSymbol(constructor);
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
        CHECK_UNDEFINED_IN_SCOPE(name);

		Symbol* member = new VariableSymbol(name, node, _enclosingFunction);
		member->setType(functionType->inputs().at(i));
		topScope()->symbols.insert(member);
		node->attachSymbol(member);
	}

	unify(node->body->type, functionType->output(), node);
    node->type = Unit;
}

void SemanticAnalyzer::visit(AssignNode* node)
{
    node->target->accept(this);
	node->value->accept(this);

    unify(node->value->type, node->target->type, node);

    node->type = Unit;
}

void SemanticAnalyzer::visit(FunctionCallNode* node)
{
	const std::string& name = node->target;
	Symbol* symbol = resolveSymbol(name);

    CHECK(symbol, "function \"{}\" is not defined", name);
    CHECK(symbol->kind == kFunction, "target of function call \"{}\" is not a function", symbol->name);

	std::vector<std::shared_ptr<Type>> paramTypes;
    for (size_t i = 0; i < node->arguments.size(); ++i)
    {
        AstNode& argument = *node->arguments[i];
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

	Symbol* symbol = resolveSymbol(name);
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
    unify(node->lhs->type, Int, node);

    node->rhs->accept(this);
    unify(node->rhs->type, Int, node);

    node->type = Bool;
}

void SemanticAnalyzer::visit(LogicalNode* node)
{
    node->lhs->accept(this);
    unify(node->lhs->type, Bool, node);

    node->rhs->accept(this);
    unify(node->rhs->type, Bool, node);

    node->type = Bool;
}

void SemanticAnalyzer::visit(BlockNode* node)
{
    for (auto& child : node->children)
    {
        child->accept(this);
        unify(child->type, Unit, node);
    }

    node->type = Unit;
}

void SemanticAnalyzer::visit(IfNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, Bool, node);

    node->body->accept(this);
    unify(node->body->type, Unit, node);

    node->type = Unit;
}

void SemanticAnalyzer::visit(IfElseNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, Bool, node);

    node->body->accept(this);
    unify(node->body->type, Unit, node);

    node->else_body->accept(this);
    unify(node->else_body->type, Unit, node);

    node->type = Unit;
}

void SemanticAnalyzer::visit(WhileNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, Bool, node);

    // Save the current inner-most loop so that we can restore it after
    // visiting the children of this loop.
    WhileNode* outerLoop = _enclosingLoop;

    _enclosingLoop = node;
    node->body->accept(this);
    _enclosingLoop = outerLoop;

    unify(node->body->type, Unit, node);

    node->type = Unit;
}

void SemanticAnalyzer::visit(BreakNode* node)
{
    CHECK(_enclosingLoop, "break statement must be within a loop");
    node->loop = _enclosingLoop;

    node->type = Unit;
}

void SemanticAnalyzer::visit(IntNode* node)
{
    node->type = Int;
}

void SemanticAnalyzer::visit(BoolNode* node)
{
    node->type = Bool;
}

void SemanticAnalyzer::visit(ReturnNode* node)
{
    CHECK(_enclosingFunction, "Cannot return from top level");

    node->expression->accept(this);

    assert(_enclosingFunction->symbol->type->tag() == ttFunction);

    // Value of expression must equal the return type of the enclosing function.
    FunctionType* functionType = _enclosingFunction->symbol->type->get<FunctionType>();

    unify(node->expression->type, functionType->output(), node);

    node->type = Unit;
}

void SemanticAnalyzer::visit(VariableNode* node)
{
    Symbol* symbol = resolveSymbol(node->name);
    CHECK(symbol != nullptr, "symbol \"{}\" is not defined in this scope", node->name);
    CHECK(symbol->kind == kVariable, "symbol \"{}\" is not a variable", node->name);

    node->symbol = symbol;
    node->type = symbol->type;
}

void SemanticAnalyzer::visit(StructDefNode* node)
{
    CHECK_TOP_LEVEL("struct declaration");

    AstVisitor::visit(node);

    // The struct name cannot have already been used for something
    const std::string& structName = node->name;
    CHECK_UNDEFINED(structName);

    // Actually create the type
    std::shared_ptr<Type> newType = StructType::create(structName, node);
    topScope()->types.insert(new TypeSymbol(structName, node, newType));

    node->structType = newType;
    node->type = Unit;
}

void SemanticAnalyzer::visit(MemberDefNode* node)
{
    // All of the constructor members must refer to already-declared types
    std::shared_ptr<Type> type = resolveTypeName(*node->typeName);
    CHECK(type, "unknown member type \"{}\"", node->typeName->str());

    node->memberType = type;
    node->type = Unit;
}

void SemanticAnalyzer::visit(StructInitNode* node)
{
    std::shared_ptr<Type> type = getBaseType(node->structName);
    CHECK(type, "unknown type \"{}\"", node->structName);
    CHECK(type->get<StructType>(), "cannot initialize non-struct type \"{}\"", node->structName);

    node->type = type;
}

void SemanticAnalyzer::visit(MemberAccessNode* node)
{
    const std::string& name = node->varName;

    Symbol* symbol = resolveSymbol(name);
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
