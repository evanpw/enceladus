#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <boost/lexical_cast.hpp>

#include "ast_context.hpp"
#include "scope.hpp"
#include "semantic.hpp"
#include "tokens.hpp"
#include "utility.hpp"

#include "lib/library.h"

//// Utility functions /////////////////////////////////////////////////////////

#define CHECK(p, ...) if (!(p)) { semanticError(node->location, __VA_ARGS__); }

#define CHECK_IN(node, p, ...) if (!(p)) { semanticError((node)->location, __VA_ARGS__); }

#define CHECK_AT(location, p, ...) if (!(p)) { semanticError((location), __VA_ARGS__); }

#define CHECK_UNDEFINED(name) \
    CHECK(!resolveSymbol(name), "symbol \"{}\" is already defined", name); \
    CHECK(!resolveTypeSymbol(name), "symbol \"{}\" is already defined", name)

#define CHECK_UNDEFINED_SYMBOL(name) \
    CHECK(!resolveSymbol(name), "symbol \"{}\" is already defined", name);

#define CHECK_UNDEFINED_TYPE(name) \
    CHECK(!resolveTypeSymbol(name), "symbol \"{}\" is already defined", name)

#define CHECK_UNDEFINED_IN(name, node) \
    CHECK_IN((node), !resolveSymbol(name), "symbol \"{}\" is already defined", name); \
    CHECK_IN((node), !resolveTypeSymbol(name), "symbol \"{}\" is already defined", name)

#define CHECK_UNDEFINED_IN_SCOPE(name) \
    CHECK(!topScope()->symbols.find(name), "symbol \"{}\" is already defined in this scope", name); \
    CHECK(!topScope()->types.find(name), "symbol \"{}\" is already defined in this scope", name)

#define CHECK_TOP_LEVEL(name) \
    CHECK(!_enclosingFunction, "{} must be at top level", name)

template<typename... Args>
void SemanticAnalyzer::semanticError(const YYLTYPE& location, const std::string& str, Args... args)
{
    std::stringstream ss;

    ss << "Near line " << location.first_line << ", "
       << "column " << location.first_column << ": "
       << format(str, args...);

    throw SemanticError(ss.str());
}

template<typename... Args>
void SemanticAnalyzer::semanticErrorNoNode(const std::string& str, Args... args)
{
    std::stringstream ss;

    ss << "" << format(str, args...);

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

SemanticAnalyzer::SemanticAnalyzer(ProgramNode* root, AstContext* context)
: _root(root)
, _context(context)
, _typeTable(context->typeTable())
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
    Scope& scope = _root->scope;

    //// Built-in types ////////////////////////////////////////////////////////
    scope.types.insert(new TypeSymbol("Int", _root, _typeTable->Int));
    scope.types.insert(new TypeSymbol("Bool", _root, _typeTable->Bool));
    scope.types.insert(new TypeSymbol("Unit", _root, _typeTable->Unit));
    scope.types.insert(new TypeSymbol("String", _root, _typeTable->String));

    scope.types.insert(new TypeConstructorSymbol("Function", _root, _typeTable->Function));
    scope.types.insert(new TypeConstructorSymbol("Array", _root, _typeTable->Array));

	//// Create symbols for built-in functions
    FunctionSymbol* notFn = makeBuiltin("not");
	notFn->setType(_typeTable->createFunctionType({_typeTable->Bool}, _typeTable->Bool));
	scope.symbols.insert(notFn);

	//// Integer arithmetic functions //////////////////////////////////////////

    Type* arithmeticType = _typeTable->createFunctionType({_typeTable->Int, _typeTable->Int}, _typeTable->Int);

	FunctionSymbol* add = makeBuiltin("+");
	add->setType(arithmeticType);
	scope.symbols.insert(add);

	FunctionSymbol* subtract = makeBuiltin("-");
	subtract->setType(arithmeticType);
	scope.symbols.insert(subtract);

	FunctionSymbol* multiply = makeBuiltin("*");
	multiply->setType(arithmeticType);
	scope.symbols.insert(multiply);

	FunctionSymbol* divide = makeBuiltin("/");
	divide->setType(arithmeticType);
	scope.symbols.insert(divide);

	FunctionSymbol* modulus = makeBuiltin("%");
	modulus->setType(arithmeticType);
	scope.symbols.insert(modulus);


	//// These definitions are only needed so that we list them as external
	//// symbols in the output assembly file. They can't be called from
	//// language.
    FunctionSymbol* gcAllocate = new FunctionSymbol("gcAllocate", _root, nullptr);
    gcAllocate->isExternal = true;
    scope.symbols.insert(gcAllocate);

    scope.symbols.insert(new FunctionSymbol("_main", _root, nullptr));
}

void SemanticAnalyzer::resolveBaseType(TypeName* typeName, std::unordered_map<std::string, Type*>& variables, bool createVariables)
{
    const std::string& name = typeName->name;

    if (!islower(name[0]))
    {
        Symbol* symbol = resolveTypeSymbol(name);
        CHECK_AT(typeName->location, symbol, "Base type \"{}\" is not defined", name);
        CHECK_AT(typeName->location, symbol->kind == kType, "Symbol \"{}\" is not a base type", name);

        typeName->type = symbol->type;
    }
    else
    {
        auto i = variables.find(name);
        CHECK_AT(typeName->location, createVariables || i != variables.end(), "No such type variable \"{}\"", name);

        if (createVariables && i == variables.end())
        {
            Type* var = _typeTable->createTypeVariable(true);
            variables[name] = var;
            typeName->type = var;
        }
        else
        {
            typeName->type = i->second;
        }
    }
}

TypeConstructor* SemanticAnalyzer::getTypeConstructor(const TypeName* typeName)
{
    const std::string& name = typeName->name;

    Symbol* symbol = resolveTypeSymbol(name);
    CHECK_AT(typeName->location, symbol, "Type constructor \"{}\" is not defined", name);
    CHECK_AT(typeName->location, symbol->kind == kTypeConstructor, "Symbol \"{}\" is not a type constructor", name);

    return symbol->asTypeConstructor()->typeConstructor;
}

void SemanticAnalyzer::resolveTypeName(TypeName* typeName, std::unordered_map<std::string, Type*>& variables, bool createVariables)
{
    if (typeName->parameters.empty())
    {
        resolveBaseType(typeName, variables, createVariables);
    }
    else
    {
        std::vector<Type*> typeParameters;
        for (auto& parameter : typeName->parameters)
        {
            resolveTypeName(parameter, variables, createVariables);
            typeParameters.push_back(parameter->type);
        }

        if (typeName->name == "Function")
        {
            // With no type parameters, the return value should be Unit
            if (typeParameters.empty())
            {
                typeParameters.push_back(_typeTable->Unit);
            }

            Type* resultType = typeParameters.back();
            typeParameters.pop_back();

            typeName->type = _typeTable->createFunctionType(typeParameters, resultType);
        }
        else
        {
            TypeConstructor* typeConstructor = getTypeConstructor(typeName);

            CHECK_AT(typeName->location,
                     typeConstructor->parameters() == typeParameters.size(),
                     "Expected {} parameter(s) to type constructor {}, but got {}",
                     typeConstructor->parameters(),
                     typeName->name,
                     typeParameters.size());

            typeName->type = _typeTable->createConstructedType(typeConstructor, typeParameters);
        }
    }
}

void SemanticAnalyzer::resolveTypeName(TypeName* typeName, bool createVariables)
{
    std::unordered_map<std::string, Type*> variables;
    resolveTypeName(typeName, variables, createVariables);
}

void SemanticAnalyzer::insertSymbol(Symbol* symbol)
{
    if (!symbol) return;

    static unsigned long count = 0;

    if (symbol->name == "_")
    {
        symbol->name = format("_unnamed{}", count++);
        return;
    }

    topScope()->symbols.insert(symbol);
}

void SemanticAnalyzer::releaseSymbol(Symbol* symbol)
{
    topScope()->symbols.release(symbol->name);
}


//// Type inference functions //////////////////////////////////////////////////

void SemanticAnalyzer::bindVariable(Type* variable, Type* value, AstNode* node)
{
    assert(variable->tag() == ttVariable);

    Type* rhs = unwrap(value);

    // Check to see if the value is actually the same type variable, and don't
    // rebind
    if (rhs->tag() == ttVariable)
    {
        if (variable->get<TypeVariable>() == rhs->get<TypeVariable>()) return;
    }

    // Make sure that the variable actually occurs in the type
    if (occurs(variable->get<TypeVariable>(), rhs))
    {
        std::stringstream ss;
        ss << "variable " << variable->name() << " already occurs in " << rhs->name();
        inferenceError(node, ss.str());
    }

    variable->get<TypeVariable>()->assign(rhs);
}

void SemanticAnalyzer::inferenceError(AstNode* node, const std::string& msg)
{
    //std::cerr << typeid(*node).name() << std::endl;

    std::stringstream ss;

    ss << "Near line " << node->location.first_line << ", "
       << "column " << node->location.first_column << ": "
       << "error: " << msg;

    throw TypeInferenceError(ss.str());
}

std::set<TypeVariable*> SemanticAnalyzer::getFreeVars(Symbol& symbol)
{
    std::set<TypeVariable*> freeVars;

    // FIXME: Some functions (like die) which cannot be called from in-language
    // have no type scheme because they don't need it. We should have a better
    // way of declaring external symbols without introducing fake in-language
    // functions.
    if (!symbol.typeScheme) return freeVars;

    freeVars += symbol.typeScheme->freeVars();

    if (symbol.kind == kFunction)
    {
        Type* type = unwrap(symbol.typeScheme->type());

        assert(type->tag() == ttFunction);
        FunctionType* functionType = type->get<FunctionType>();
        for (auto& type : functionType->inputs())
        {
            freeVars += type->freeVars();
        }
        freeVars += functionType->output()->freeVars();
    }

    return freeVars;
}

TypeScheme* SemanticAnalyzer::generalize(Type* type, const std::vector<Scope*>& scopes)
{
    std::set<TypeVariable*> typeFreeVars = type->freeVars();

    std::set<TypeVariable*> envFreeVars;
    for (auto i = scopes.rbegin(); i != scopes.rend(); ++i)
    {
        Scope* scope = *i;
        for (auto& j : scope->symbols.symbols)
        {
            const std::unique_ptr<Symbol>& symbol = j.second;
            envFreeVars += getFreeVars(*symbol);
        }
    }

    return type->table()->createTypeScheme(type, typeFreeVars - envFreeVars);
}

Type* SemanticAnalyzer::instantiate(Type* type, const std::map<TypeVariable*, Type*>& replacements)
{
    Type* realType = unwrap(type);

    switch (realType->tag())
    {
        case ttBase:
            return realType;

        case ttVariable:
        {
            TypeVariable* typeVariable = realType->get<TypeVariable>();

            auto i = replacements.find(typeVariable);
            if (i != replacements.end())
            {
                return i->second;
            }
            else
            {
                return realType;
            }
        }

        case ttFunction:
        {
            FunctionType* functionType = realType->get<FunctionType>();

            std::vector<Type*> newInputs;
            for (Type* input : functionType->inputs())
            {
                newInputs.push_back(instantiate(input, replacements));
            }

            return _typeTable->createFunctionType(newInputs, instantiate(functionType->output(), replacements));
        }

        case ttConstructed:
        {
            std::vector<Type*> params;

            ConstructedType* constructedType = realType->get<ConstructedType>();
            for (Type* parameter : constructedType->typeParameters())
            {
                params.push_back(instantiate(parameter, replacements));
            }

            return _typeTable->createConstructedType(constructedType->typeConstructor(), params);
        }
    }

    assert(false);
    return nullptr;
}

Type* SemanticAnalyzer::instantiate(TypeScheme* scheme)
{
    std::map<TypeVariable*, Type*> replacements;
    for (TypeVariable* boundVar : scheme->quantified())
    {
        replacements[boundVar] = _typeTable->createTypeVariable();
    }

    return instantiate(scheme->type(), replacements);
}

bool SemanticAnalyzer::occurs(TypeVariable* variable, Type* value)
{
    Type* rhs = unwrap(value);

    switch (rhs->tag())
    {
        case ttBase:
            return false;

        case ttVariable:
        {
            TypeVariable* typeVariable = rhs->get<TypeVariable>();
            return typeVariable == variable;
        }

        case ttFunction:
        {
            FunctionType* functionType = rhs->get<FunctionType>();

            for (auto& input : functionType->inputs())
            {
                if (occurs(variable, input)) return true;
            }

            return occurs(variable, functionType->output());
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = rhs->get<ConstructedType>();
            for (Type* parameter : constructedType->typeParameters())
            {
                if (occurs(variable, parameter)) return true;
            }

            return false;
        }
    }

    assert(false);
}

void SemanticAnalyzer::unify(Type* a, Type* b, AstNode* node)
{
    Type* lhs = unwrap(a);
    Type* rhs = unwrap(b);

    // std::cerr << "unify: "
    //           << lhs->name() << " " << lhs->isVariable() << ((lhs->isVariable()) ? lhs->get<TypeVariable>()->rigid() : false)
    //           << " -- "
    //           << rhs->name() << " " << rhs->isVariable() << ((rhs->isVariable()) ? rhs->get<TypeVariable>()->rigid() : false)
    //           << std::endl;

    assert(lhs && rhs && node);

    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        // Two base types can be unified only if equal (we don't have inheritance)
        if (lhs->name() == rhs->name())
            return;
    }
    else if (lhs->tag() == ttVariable)
    {
        // Non-rigid type variables can always be bound
        if (!lhs->get<TypeVariable>()->rigid())
        {
            bindVariable(lhs, rhs, node);
            return;
        }
        else
        {
            // Trying to unify a rigid type variable with a type that is not a
            // variable is always an error
            if (rhs->tag() == ttVariable)
            {
                // A rigid type variable unifies with itself
                if (lhs->get<TypeVariable>() == rhs->get<TypeVariable>())
                {
                    return;
                }

                // And non-rigid type variables can be bound to rigid ones
                else if (!rhs->get<TypeVariable>()->rigid())
                {
                    bindVariable(rhs, lhs, node);
                    return;
                }
             }
        }
    }
    else if (rhs->tag() == ttVariable && !rhs->get<TypeVariable>()->rigid())
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
    enterScope(&node->scope);
	injectSymbols();

	//// Recurse down into children
	AstVisitor::visit(node);

    for (auto& child : node->children)
    {
        unify(child->type, _typeTable->Unit, child);
    }

    exitScope();

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(ConstructorSpec* node)
{
    CHECK_UNDEFINED_SYMBOL(node->name);

    // Resolve member types
    for (auto& member : node->members)
    {
        resolveTypeName(member, node->typeContext);
        node->memberTypes.push_back(member->type);
    }

    // Create a symbol for the constructor
    FunctionSymbol* symbol = new FunctionSymbol(node->name, node, nullptr);
    symbol->isForeign = true;
    std::set<TypeVariable*> variables;
    for (auto& element : node->typeContext)
    {
        variables.insert(element.second->get<TypeVariable>());
    }
    symbol->setTypeScheme(_typeTable->createTypeScheme(_typeTable->createFunctionType(node->memberTypes, node->resultType), variables));
    insertSymbol(symbol);

    ValueConstructor* valueConstructor = _typeTable->createValueConstructor(symbol, node->memberTypes);
    node->valueConstructor = valueConstructor;
    node->resultType->addValueConstructor(valueConstructor);

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(DataDeclaration* node)
{
	// Data declarations cannot be local
    CHECK_TOP_LEVEL("data declaration");

	// The data type name cannot have already been used for something
	const std::string& name = node->name;
    CHECK_UNDEFINED(name);

    // Actually create the type
    if (node->typeParameters.empty())
    {
        Type* newType = _typeTable->createBaseType(node->name);
        topScope()->types.insert(new TypeSymbol(name, node, newType));

        for (auto& spec : node->constructorSpecs)
        {
            spec->resultType = newType;
            spec->accept(this);
            node->valueConstructors.push_back(spec->valueConstructor);
        }
    }
    else
    {
        // Create type variables for each type parameter
        std::vector<Type*> variables;
        std::unordered_map<std::string, Type*> typeContext;
        for (auto& typeParameter : node->typeParameters)
        {
            Type* var = _typeTable->createTypeVariable(true);
            variables.push_back(var);
            typeContext.emplace(typeParameter, var);
        }

        TypeConstructor* typeConstructor = _typeTable->createTypeConstructor(name, node->typeParameters.size());
        TypeConstructorSymbol* symbol = new TypeConstructorSymbol(name, node, typeConstructor);
        topScope()->types.insert(symbol);

        Type* newType = _typeTable->createConstructedType(typeConstructor, variables);

        for (auto& spec : node->constructorSpecs)
        {
            spec->typeContext = typeContext;
            spec->resultType = newType;
            spec->accept(this);
            typeConstructor->addValueConstructor(spec->valueConstructor);
            node->valueConstructors.push_back(spec->valueConstructor);
        }
    }

	node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(TypeAliasNode* node)
{
    // Type aliases cannot be local
    CHECK_TOP_LEVEL("type alias declaration");

    // The new type name cannot have already been used
    const std::string& typeName = node->name;
    CHECK_UNDEFINED(typeName);

    resolveTypeName(node->underlying);

    // Insert the alias into the type table
    topScope()->types.insert(new TypeSymbol(typeName, node, node->underlying->type));

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(FunctionDefNode* node)
{
	// Functions cannot be declared inside of another function
    CHECK(!_enclosingFunction, "functions cannot be nested");

	// The function name cannot have already been used as something else
	const std::string& name = node->name;
    CHECK_UNDEFINED(name);

    resolveTypeName(node->typeName, true);
    Type* type = unwrap(node->typeName->type);
    FunctionType* functionType = type->get<FunctionType>();
    node->functionType = functionType;

    assert(functionType->inputs().size() == node->params.size());

    const std::vector<Type*>& paramTypes = functionType->inputs();

	// The type scheme of this function is temporarily not generalized. We
	// want to infer whatever concrete types we can within the body of the
	// function.
	Symbol* symbol = new FunctionSymbol(name, node, node);
	symbol->setType(type);
	insertSymbol(symbol);
	node->symbol = symbol;

	enterScope(&node->scope);

	// Add symbols corresponding to the formal parameters to the
	// function's scope
	for (size_t i = 0; i < node->params.size(); ++i)
	{
		const std::string& param = node->params[i];

		Symbol* paramSymbol = new VariableSymbol(param, node, node);
		paramSymbol->asVariable()->isParam = true;
        paramSymbol->asVariable()->offset = i;
		paramSymbol->setType(paramTypes[i]);
		insertSymbol(paramSymbol);

        node->parameterSymbols.push_back(paramSymbol);
	}

	// Recurse
	_enclosingFunction = node;
	node->body->accept(this);
	_enclosingFunction = nullptr;

	exitScope();

	// Once we've looked through the body of the function and inferred whatever
	// types we can, any remaining type variables can be generalized.
	releaseSymbol(symbol);
	symbol->setTypeScheme(generalize(symbol->typeScheme->type(), _scopes));
	insertSymbol(symbol);

    //std::cerr << name << " :: " << symbol->typeScheme->name() << std::endl;

    unify(node->body->type, functionType->output(), node);
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(ForeignDeclNode* node)
{
	// Functions cannot be declared inside of another function
    CHECK_TOP_LEVEL("foreign function declaration");

	// The function name cannot have already been used as something else
	const std::string& name = node->name;
    CHECK_UNDEFINED(name);

	// We currently only support 6 function arguments for foreign functions
	// (so that we only have to pass arguments in registers)
    CHECK(node->params.size() <= 6, "a maximum of 6 arguments is supported for foreign functions");

    resolveTypeName(node->typeName, true);
    Type* functionType = unwrap(node->typeName->type);
    assert(functionType->get<FunctionType>()->inputs().size() == node->params.size());

	FunctionSymbol* symbol = new FunctionSymbol(name, node, nullptr);
	symbol->setTypeScheme(generalize(functionType, _scopes));
	symbol->isForeign = true;
	symbol->isExternal = true;
	insertSymbol(symbol);
	node->symbol = symbol;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(LetNode* node)
{
	// Visit children. Do this first so that we can't have recursive definitions.
	AstVisitor::visit(node);

	const std::string& target = node->target;
    if (target != "_")
    {
        CHECK_UNDEFINED_IN_SCOPE(target);

    	Symbol* symbol = new VariableSymbol(target, node, _enclosingFunction);

    	if (node->typeName)
    	{
    		resolveTypeName(node->typeName);
    		symbol->setType(node->typeName->type);
    	}
    	else
    	{
    		symbol->setType(_typeTable->createTypeVariable());
    	}

    	insertSymbol(symbol);
    	node->symbol = symbol;

    	unify(node->rhs->type, symbol->type, node);
    }
    else
    {
        node->symbol = nullptr;
    }

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(SwitchNode* node)
{
    node->expr->accept(this);
    Type* type = node->expr->type;

    node->type = _typeTable->createTypeVariable();

    std::set<size_t> constructorTags;
    for (auto& arm : node->arms)
    {
        arm->matchType = type;
        arm->accept(this);

        bool notDuplicate = (constructorTags.find(arm->constructorTag) == constructorTags.end());
        CHECK_AT(arm->location, notDuplicate, "cannot repeat constructors in match statement");
        constructorTags.insert(arm->constructorTag);

        unify(node->type, arm->type, node);
    }

    CHECK(constructorTags.size() == type->valueConstructors().size(), "switch statement is not exhaustive");
}

void SemanticAnalyzer::visit(MatchArm* node)
{
    const std::string& constructorName = node->constructor;
    std::pair<size_t, ValueConstructor*> result = node->matchType->getValueConstructor(constructorName);
    CHECK(result.second, "type \"{}\" has no value constructor named \"{}\"", node->matchType->name(), constructorName);
    node->constructorTag = result.first;
    node->valueConstructor = result.second;

    Symbol* constructorSymbol = resolveSymbol(constructorName);
    CHECK(constructorSymbol, "constructor \"{}\" is not defined", constructorName);

    // A symbol with a capital letter should always be a constructor
    assert(constructorSymbol->kind == kFunction);
    assert(constructorSymbol->typeScheme->tag() == ttFunction);

    Type* instantiatedType = instantiate(constructorSymbol->typeScheme);
    FunctionType* functionType = instantiatedType->get<FunctionType>();
    Type* constructedType = functionType->output();
    unify(constructedType, node->matchType, node);

    CHECK(functionType->inputs().size() == node->params.size(),
        "constructor pattern \"{}\" does not have the correct number of arguments", constructorName);

    enterScope(&node->bodyScope);

    // And create new variables for each of the members of the constructor
    for (size_t i = 0; i < node->params.size(); ++i)
    {
        const std::string& name = node->params[i];
        CHECK_UNDEFINED_IN_SCOPE(name);

        if (name != "_")
        {
            Symbol* member = new VariableSymbol(name, node, _enclosingFunction);
            member->setType(functionType->inputs().at(i));
            insertSymbol(member);
            node->symbols.push_back(member);
        }
        else
        {
            node->symbols.push_back(nullptr);
        }
    }

    // Visit body
    AstVisitor::visit(node);

    exitScope();

    node->type = node->body->type;
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
    Type* instantiatedType = instantiate(constructorSymbol->typeScheme);
	FunctionType* functionType = instantiatedType->get<FunctionType>();
	const Type* constructedType = functionType->output();

    CHECK(constructedType->valueConstructors().size() == 1, "let statement pattern matching only applies to types with a single constructor");
    CHECK(functionType->inputs().size() == node->params.size(), "constructor pattern \"{}\" does not have the correct number of arguments", constructor);

    node->valueConstructor = constructedType->valueConstructors()[0];

	// And create new variables for each of the members of the constructor
	for (size_t i = 0; i < node->params.size(); ++i)
	{
		const std::string& name = node->params[i];
        CHECK_UNDEFINED_IN_SCOPE(name);

        if (name != "_")
        {
    		Symbol* member = new VariableSymbol(name, node, _enclosingFunction);
    		member->setType(functionType->inputs().at(i));
    		insertSymbol(member);
    		node->symbols.push_back(member);
        }
        else
        {
            node->symbols.push_back(nullptr);
        }
	}

	unify(node->body->type, functionType->output(), node);
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(AssignNode* node)
{
    Symbol* symbol = resolveSymbol(node->target);
    CHECK(symbol != nullptr, "symbol \"{}\" is not defined in this scope. Did you mean to define it here?", node->target);
    CHECK(symbol->kind == kVariable, "symbol \"{}\" is not a variable", node->target);
    node->symbol = symbol;

	node->rhs->accept(this);

    unify(node->rhs->type, node->symbol->type, node);

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(FunctionCallNode* node)
{
	const std::string& name = node->target;
	Symbol* symbol = resolveSymbol(name);

    CHECK(symbol, "function \"{}\" is not defined", name);

	std::vector<Type*> paramTypes;
    for (size_t i = 0; i < node->arguments.size(); ++i)
    {
        AstNode& argument = *node->arguments[i];
        argument.accept(this);

        paramTypes.push_back(argument.type);
    }

	node->symbol = symbol;

    Type* returnType = _typeTable->createTypeVariable();
    Type* functionType = instantiate(symbol->typeScheme);

    unify(functionType, _typeTable->createFunctionType(paramTypes, returnType), node);

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
        node->kind = NullaryNode::VARIABLE;
	}
	else /* symbol->kind == kFunction */
	{
		node->symbol = symbol;
        Type* functionType = instantiate(symbol->typeScheme);

        FunctionSymbol* functionSymbol = symbol->asFunction();
        if (functionType->get<FunctionType>()->inputs().empty())
        {
            Type* returnType = _typeTable->createTypeVariable();
            unify(functionType, _typeTable->createFunctionType({}, returnType), node);

            node->type = returnType;

            if (functionSymbol->isForeign)
            {
                node->kind = NullaryNode::FOREIGN_CALL;
            }
            else
            {
                node->kind = NullaryNode::FUNC_CALL;
            }
        }
        else
        {
            node->type = functionType;
            node->kind = NullaryNode::CLOSURE;
        }
	}
}

void SemanticAnalyzer::visit(ComparisonNode* node)
{
    node->lhs->accept(this);
    unify(node->lhs->type, _typeTable->Int, node);

    node->rhs->accept(this);
    unify(node->rhs->type, _typeTable->Int, node);

    node->type = _typeTable->Bool;
}

void SemanticAnalyzer::visit(LogicalNode* node)
{
    node->lhs->accept(this);
    unify(node->lhs->type, _typeTable->Bool, node);

    node->rhs->accept(this);
    unify(node->rhs->type, _typeTable->Bool, node);

    node->type = _typeTable->Bool;
}

void SemanticAnalyzer::visit(BlockNode* node)
{
    // Blocks have the type of their last expression

    for (size_t i = 0; i + 1 < node->children.size(); ++i)
    {
        node->children[i]->accept(this);
        unify(node->children[i]->type, _typeTable->Unit, node);
    }

    if (node->children.size() > 0)
    {
        node->children.back()->accept(this);
        node->type = node->children.back()->type;
    }
    else
    {
        node->type = _typeTable->Unit;
    }
}

void SemanticAnalyzer::visit(IfNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, _typeTable->Bool, node);

    enterScope(&node->bodyScope);
    node->body->accept(this);
    exitScope();
    unify(node->body->type, _typeTable->Unit, node);

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(IfElseNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, _typeTable->Bool, node);

    enterScope(&node->bodyScope);
    node->body->accept(this);
    exitScope();

    enterScope(&node->elseScope);
    node->elseBody->accept(this);
    exitScope();

    unify(node->body->type, node->elseBody->type, node);
    node->type = node->body->type;
}

void SemanticAnalyzer::visit(WhileNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, _typeTable->Bool, node);

    // Save the current inner-most loop so that we can restore it after
    // visiting the children of this loop.
    LoopNode* outerLoop = _enclosingLoop;

    node->type = _typeTable->Unit;

    _enclosingLoop = node;
    enterScope(&node->bodyScope);

    node->body->accept(this);

    exitScope();
    _enclosingLoop = outerLoop;

    unify(node->body->type, _typeTable->Unit, node);
}

void SemanticAnalyzer::visit(ForeverNode* node)
{
    // Save the current inner-most loop so that we can restore it after
    // visiting the children of this loop.
    LoopNode* outerLoop = _enclosingLoop;

    // The type of an infinite loop can be anything, unless we encounter a break
    // statement in the body, in which case it must be Unit
    node->type = _typeTable->createTypeVariable();

    _enclosingLoop = node;
    enterScope(&node->bodyScope);

    node->body->accept(this);

    exitScope();
    _enclosingLoop = outerLoop;

    unify(node->body->type, _typeTable->Unit, node);
}

void SemanticAnalyzer::visit(BreakNode* node)
{
    CHECK(_enclosingLoop, "break statement must be within a loop");

    unify(_enclosingLoop->type, _typeTable->Unit, node);

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(IntNode* node)
{
    node->type = _typeTable->Int;
}

void SemanticAnalyzer::visit(BoolNode* node)
{
    node->type = _typeTable->Bool;
}

void SemanticAnalyzer::visit(StringLiteralNode* node)
{
    node->type = _typeTable->String;

    std::string name = "__staticString" + std::to_string(node->counter);
    VariableSymbol* symbol = new VariableSymbol(name, node, nullptr);
    symbol->isStatic = true;
    symbol->contents = node->content;
    insertSymbol(symbol);
    node->symbol = symbol;
}

void SemanticAnalyzer::visit(ReturnNode* node)
{
    CHECK(_enclosingFunction, "Cannot return from top level");

    node->expression->accept(this);

    Type* type = unwrap(_enclosingFunction->symbol->type);
    assert(type->tag() == ttFunction);

    // Value of expression must equal the return type of the enclosing function.
    FunctionType* functionType = type->get<FunctionType>();

    unify(node->expression->type, functionType->output(), node);

    // Since the function doesn't continue past a return statement, its value
    // doesn't matter
    node->type = _typeTable->createTypeVariable();
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
    // Struct definitions cannot be local
    CHECK_TOP_LEVEL("struct declaration");

    AstVisitor::visit(node);

    // The type name cannot have already been used for something
    const std::string& typeName = node->name;
    CHECK_UNDEFINED(typeName);

    CHECK(!node->members.empty(), "structs cannot be empty");

    Type* newType = _typeTable->createBaseType(node->name);
    topScope()->types.insert(new TypeSymbol(typeName, node, newType));

    std::vector<std::string> memberNames;
    std::vector<Type*> memberTypes;
    std::vector<MemberSymbol*> memberSymbols;
    for (auto& member : node->members)
    {
        CHECK_UNDEFINED_IN(member->name, member);

        memberTypes.push_back(member->memberType);
        memberNames.push_back(member->name);

        MemberSymbol* memberSymbol = new MemberSymbol(member->name, node);
        memberSymbol->setType(_typeTable->createFunctionType({newType}, member->memberType));
        insertSymbol(memberSymbol);
        memberSymbols.push_back(memberSymbol);
    }

    // Create a symbol for the constructor
    FunctionSymbol* symbol = new FunctionSymbol(typeName, node, nullptr);
    symbol->isForeign = true;
    symbol->setType(_typeTable->createFunctionType(memberTypes, newType));
    insertSymbol(symbol);

    ValueConstructor* valueConstructor = _typeTable->createValueConstructor(symbol, memberTypes, memberNames);
    node->valueConstructor = valueConstructor;
    newType->addValueConstructor(valueConstructor);

    assert(valueConstructor->members().size() == memberSymbols.size());
    for (size_t i = 0; i < memberSymbols.size(); ++i)
    {
        ValueConstructor::MemberDesc& member = valueConstructor->members().at(i);
        assert(member.name == memberNames[i]);

        memberSymbols[i]->location = member.location;
    }

    node->structType = newType;
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(MemberDefNode* node)
{
    // All of the constructor members must refer to already-declared types
    resolveTypeName(node->typeName);
    node->memberType = node->typeName->type;
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(MemberAccessNode* node)
{
    const std::string& name = node->varName;

    Symbol* varSymbol = resolveSymbol(name);
    CHECK(varSymbol, "symbol \"{}\" is not defined in this scope", name);
    CHECK(varSymbol->kind == kVariable, "\"{}\" is not the name of a variable", name);
    node->varSymbol = varSymbol->asVariable();

    CHECK(node->memberName != "_", "member access syntax cannot be used with unnamed members");
    Symbol* memberSymbol = resolveSymbol(node->memberName);
    CHECK(memberSymbol, "symbol \"{}\" is not defined in this scope", name);
    CHECK(memberSymbol->kind == kMember, "\"{}\" is not the name of a data type member", node->memberName);
    node->memberSymbol = memberSymbol->asMember();

    Type* varType = varSymbol->type;
    Type* memberType = instantiate(memberSymbol->typeScheme);
    Type* returnType = _typeTable->createTypeVariable();
    unify(memberType, _typeTable->createFunctionType({varType}, returnType), node);

    node->type = returnType;
    node->memberLocation = memberSymbol->asMember()->location;
}
