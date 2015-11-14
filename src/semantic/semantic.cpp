#include "semantic/semantic.hpp"

#include "ast/ast_context.hpp"
#include "lib/library.h"
#include "semantic/subtype.hpp"
#include "semantic/unify_trait.hpp"
#include "semantic/type_functions.hpp"
#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <boost/lexical_cast.hpp>


//// Type inference functions //////////////////////////////////////////////////

static void inferenceError(AstNode* node, const std::string& msg)
{
    std::stringstream ss;

    auto location = node->location;

    ss << location.filename << ":" << location.first_line <<  ":" << location.first_column
       << ": " << msg;

    throw TypeInferenceError(ss.str());
}

static void unify(Type* lhs, Type* rhs, AstNode* node)
{
    auto result = tryUnify(lhs, rhs);

    if (!result.first)
    {
        if (!result.second.empty())
        {
            inferenceError(node, result.second);
        }
        else
        {
            std::stringstream ss;
            ss << "cannot unify types " << lhs->str() << " and " << rhs->str();
            inferenceError(node, ss.str());
        }
    }
}

static void unify(Type* lhs, Trait* rhs, AstNode* node)
{
    auto result = tryUnify(lhs, rhs);

    if (!result.first)
    {
        if (!result.second.empty())
        {
            inferenceError(node, result.second);
        }
        else
        {
            std::stringstream ss;
            ss << "cannot unify type " << lhs->str() << " with trait " << rhs->str();
            inferenceError(node, ss.str());
        }
    }
}

static void imposeConstraint(Type* type, Trait* trait, AstNode* node)
{
    if (type->isVariable())
    {
        TypeVariable* var = type->get<TypeVariable>();

        auto& constraints = var->constraints();

        bool matches = constraints.find(trait) != constraints.end();

        if (!matches)
        {
            // A quantified type variable can't acquire any new constraints in the
            // process of unification (see overrideType test)
            if (var->quantified() && !isSubtype(type, trait))
            {
                std::stringstream ss;
                ss << "Type variable " << type->str() << " does not satisfy constraint " << trait->str();
                inferenceError(node, ss.str());
            }
            else
            {
                var->addConstraint(trait);
            }
        }

        return;
    }

    if (!isSubtype(type, trait))
    {
        std::stringstream ss;
        ss << "Type " << type->str() << " is not an instance of trait " << trait->str();
        inferenceError(node, ss.str());
    }
}


//// Utility functions /////////////////////////////////////////////////////////

#define CHECK(p, ...) if (!(p)) { semanticError(node->location, __VA_ARGS__); }

#define CHECK_IN(node, p, ...) if (!(p)) { semanticError((node)->location, __VA_ARGS__); }

#define CHECK_AT(location, p, ...) if (!(p)) { semanticError((location), __VA_ARGS__); }

#define CHECK_UNDEFINED(name) \
    CHECK(!resolveSymbol(name), "symbol `{}` is already defined", name); \
    CHECK(!resolveTypeSymbol(name), "symbol `{}` is already defined", name)

#define CHECK_UNDEFINED_SYMBOL(name) \
    CHECK(!resolveSymbol(name), "symbol `{}` is already defined", name);

#define CHECK_UNDEFINED_TYPE(name) \
    CHECK(!resolveTypeSymbol(name), "symbol `{}` is already defined", name)

#define CHECK_UNDEFINED_IN(name, node) \
    CHECK_IN((node), !resolveSymbol(name), "symbol `{}` is already defined", name); \
    CHECK_IN((node), !resolveTypeSymbol(name), "symbol `{}` is already defined", name)

#define CHECK_UNDEFINED_IN_SCOPE(name) \
    CHECK(!_symbolTable->findTopScope(name), "symbol `{}` is already defined in this scope", name); \
    CHECK(!_symbolTable->findTopScope(name, SymbolTable::TYPE), "symbol `{}` is already defined in this scope", name)

#define CHECK_TOP_LEVEL(name) \
    CHECK(!_enclosingFunction, "{} must be at top level", name)

template<typename... Args>
void semanticError(const YYLTYPE& location, const std::string& str, Args... args)
{
    std::stringstream ss;

    ss << location.filename << ":" << location.first_line << ":" << location.first_column
       << ": " << format(str, args...);

    throw SemanticError(ss.str());
}

static std::string instanceLocation(TraitSymbol* traitSymbol, Type* type)
{
    std::stringstream ss;

    TraitInstance* instance = traitSymbol->getInstance(type);
    if (!instance)
    {
        return "(builtin)";
    }
    else
    {
        AstNode* impl = instance->implNode;
        auto& location = impl->location;

        std::stringstream ss;
        ss << location.filename << ":" << location.first_line << ":" << location.first_column;
        return ss.str();
    }
}

Symbol* SemanticAnalyzer::resolveSymbol(const std::string& name)
{
    return _symbolTable->find(name);
}

Symbol* SemanticAnalyzer::resolveTypeSymbol(const std::string& name)
{
    return _symbolTable->find(name, SymbolTable::TYPE);
}

SemanticAnalyzer::SemanticAnalyzer(AstContext* context)
: _root(context->root())
, _context(context)
, _typeTable(context->typeTable())
, _symbolTable(context->symbolTable())
, _enclosingFunction(nullptr)
, _enclosingLoop(nullptr)
, _enclosingImplNode(nullptr)
{
}

bool SemanticAnalyzer::analyze()
{
	try
	{
        _symbolTable->pushScope();

		_root->accept(this);
        checkTraitCoherence();

        _symbolTable->popScope();
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

    return true;
}

FunctionSymbol* SemanticAnalyzer::createBuiltin(const std::string& name)
{
    FunctionSymbol* symbol = _symbolTable->createFunctionSymbol(name, _root, nullptr);
    symbol->isBuiltin = true;

    return symbol;
}

FunctionSymbol* SemanticAnalyzer::createExternal(const std::string& name)
{
    FunctionSymbol* symbol = _symbolTable->createFunctionSymbol(name, _root, nullptr);
    symbol->isExternal = true;

    return symbol;
}

void SemanticAnalyzer::injectSymbols()
{
    //// Dummy symbols /////////////////////////////////////////////////////////
    _symbolTable->createDummySymbol("Else", _root);


    //// Built-in types ////////////////////////////////////////////////////////
    _symbolTable->createTypeSymbol("Int", _root, _typeTable->Int);
    _symbolTable->createTypeSymbol("UInt", _root, _typeTable->UInt);
    _symbolTable->createTypeSymbol("UInt8", _root, _typeTable->UInt8);

    Type* Self = _typeTable->createTypeVariable("Self", true);
    _symbolTable->createTraitSymbol("Num", nullptr, _typeTable->Num, Self);

    _symbolTable->createTypeSymbol("Bool", _root, _typeTable->Bool);
    _symbolTable->createTypeSymbol("Unit", _root, _typeTable->Unit);

    _symbolTable->createTypeSymbol("Function", _root, _typeTable->Function);
    _symbolTable->createTypeSymbol("Array", _root, _typeTable->Array);


	//// Create symbols for built-in functions
    FunctionSymbol* notFn = createBuiltin("not");
	notFn->type = _typeTable->createFunctionType({_typeTable->Bool}, _typeTable->Bool);

    FunctionSymbol* unsafeEmptyArray = createBuiltin("unsafeEmptyArray");
    FunctionSymbol* unsafeZeroArray = createBuiltin("unsafeZeroArray");
    Type* T = _typeTable->createTypeVariable("T", true);
    Type* ArrayT = _typeTable->Array->get<ConstructedType>()->instantiate({T});
    unsafeEmptyArray->type = _typeTable->createFunctionType({_typeTable->UInt}, ArrayT);
    unsafeZeroArray->type = _typeTable->createFunctionType({_typeTable->UInt}, ArrayT);

    FunctionSymbol* arrayLength = createBuiltin("arrayLength");
    arrayLength->type = _typeTable->createFunctionType({ArrayT}, _typeTable->UInt);

    FunctionSymbol* unsafeArrayAt = createBuiltin("unsafeArrayAt");
    unsafeArrayAt->type = _typeTable->createFunctionType({ArrayT, _typeTable->UInt}, T);

    FunctionSymbol* unsafeArraySet = createBuiltin("unsafeArraySet");
    unsafeArraySet->type = _typeTable->createFunctionType({ArrayT, _typeTable->UInt, T}, _typeTable->Unit);


	//// These definitions are only needed so that we list them as external
	//// symbols in the output assembly file. They can't be called from
	//// language.
    FunctionSymbol* gcAllocate = _symbolTable->createFunctionSymbol("gcAllocate", _root, nullptr);
    gcAllocate->isExternal = true;

    _symbolTable->createFunctionSymbol("_main", _root, nullptr);
}

Type* SemanticAnalyzer::findInContext(const std::string& varName)
{
    for (auto i = _typeContexts.rbegin(); i != _typeContexts.rend(); ++i)
    {
        TypeContext& context = *i;

        auto j = context.find(varName);
        if (j != context.end())
        {
            return j->second;
        }
    }

    return nullptr;
}

void SemanticAnalyzer::pushTypeContext(TypeContext&& typeContext)
{
    _typeContexts.emplace_back(typeContext);
    _inferredVars.push_back({});
}

void SemanticAnalyzer::popTypeContext()
{
    _inferredVars.pop_back();
    _typeContexts.pop_back();
}

void SemanticAnalyzer::resolveBaseType(TypeName* typeName, bool inferVariables)
{
    const std::string& name = typeName->name;

    Type* contextMatch = findInContext(name);
    if (contextMatch)
    {
        typeName->type = contextMatch;
        return;
    }

    if (name.size() == 1)
    {
        CHECK_AT(typeName->location, inferVariables, "Type variable `{}` is not defined", name);

        Type* var = _typeTable->createTypeVariable(name, true);

        _typeContexts.back()[name] = var;
        _inferredVars.back().emplace(var);
        typeName->type = var;
        return;
    }

    Symbol* symbol = resolveTypeSymbol(name);
    CHECK_AT(typeName->location, symbol, "Base type `{}` is not defined", name);
    CHECK_AT(typeName->location, symbol->kind == kType, "Symbol `{}` is not a base type", name);

    typeName->type = symbol->type;
}

Type* SemanticAnalyzer::getConstructedType(const TypeName* typeName)
{
    return getConstructedType(typeName->location, typeName->name);
}

Type* SemanticAnalyzer::getConstructedType(const YYLTYPE& location, const std::string& name)
{
    Symbol* symbol = resolveTypeSymbol(name);
    CHECK_AT(location, symbol, "Constructed type `{}` is not defined", name);
    CHECK_AT(location, symbol->kind == kType, "Symbol `{}` is not a type", name);
    CHECK_AT(location, symbol->type->tag() == ttConstructed, "Symbol `{}` is not a constructed type", name);

    return symbol->type;
}

void SemanticAnalyzer::resolveTypeName(TypeName* typeName, bool inferVariables)
{
    if (typeName->parameters.empty())
    {
        resolveBaseType(typeName, inferVariables);
    }
    else
    {
        std::vector<Type*> typeParameters;
        for (auto& parameter : typeName->parameters)
        {
            resolveTypeName(parameter, inferVariables);
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
            Type* type = getConstructedType(typeName);
            ConstructedType* constructedType = type->get<ConstructedType>();

            CHECK_AT(typeName->location,
                     constructedType->typeParameters().size() == typeParameters.size(),
                     "Expected {} parameter(s) to type constructor {}, but got {}",
                     constructedType->typeParameters().size(),
                     typeName->name,
                     typeParameters.size());

            TypeAssignment typeMapping;
            for (size_t i = 0; i < constructedType->typeParameters().size(); ++i)
            {
                TypeVariable* variable = constructedType->typeParameters()[i]->get<TypeVariable>();
                assert(variable && variable->quantified());
                Type* value = typeParameters[i];

                // Check constraints
                for (Trait* constraint : variable->constraints())
                {
                    if (!isSubtype(value, constraint))
                    {
                        // Inferred variables can acquire new constraints through use in other type names
                        if (inferVariables && _inferredVars.back().count(value) > 0)
                        {
                            value->get<TypeVariable>()->addConstraint(constraint);
                        }
                        else
                        {
                            semanticError(typeName->location,
                                "`{}` is not an instance of trait `{}`",
                                 value->str(),
                                 constraint->str());
                        }
                    }
                }

                typeMapping[variable] = value;
            }

            typeName->type = instantiate(type, typeMapping);
        }
    }
}

TraitSymbol* SemanticAnalyzer::resolveTrait(TypeName* traitName, std::vector<Type*>& traitParams, bool inferVariables)
{
    Symbol* symbol = resolveTypeSymbol(traitName->name);
    CHECK_AT(traitName->location, symbol, "no such trait `{}`", traitName->name);
    CHECK_AT(traitName->location, symbol->kind == kTrait, "`{}` is not a trait", traitName->name);

    TraitSymbol* traitSymbol = dynamic_cast<TraitSymbol*>(symbol);
    assert(traitSymbol);

    CHECK_AT(traitName->location, traitName->parameters.size() <= traitSymbol->typeParameters.size(), "`{}` has too many parameters", traitName->str());
    CHECK_AT(traitName->location, traitName->parameters.size() >= traitSymbol->typeParameters.size(), "`{}` has too few parameters", traitName->str());

    for (auto& traitParam : traitName->parameters)
    {
        resolveTypeName(traitParam, inferVariables);
        traitParams.push_back(traitParam->type);
    }

    return traitSymbol;
}

std::pair<bool, std::string> SemanticAnalyzer::addConstraints(Type* lhs, const std::vector<TypeName*>& constraints, bool inferVariables)
{
    assert(lhs->isVariable());
    TypeVariable* var = lhs->get<TypeVariable>();

    for (TypeName* constraint : constraints)
    {
        std::vector<Type*> traitParams;
        TraitSymbol* constraintSymbol = resolveTrait(constraint, traitParams, true);

        Trait* newConstraint = constraintSymbol->trait->instantiate(std::move(traitParams));

        auto& oldConstraints = var->constraints();

        bool found = false;
        for (Trait* oldConstraint : oldConstraints)
        {
            if (oldConstraint->prototype() == newConstraint->prototype())
            {
                found = true;

                auto result = tryUnify(oldConstraint, newConstraint);
                if (!result.first)
                {
                    std::string msg = format("can't add constraint `{}` to type variable `{}`: conflicts with existing constraint `{}`", newConstraint->str(), var->name(), oldConstraint->str());
                    return {false, msg};
                }

                break;
            }
        }

        if (!found)
        {
            var->addConstraint(newConstraint);
        }
    }

    return {true, ""};
}

void SemanticAnalyzer::resolveTypeParams(AstNode* node, const std::vector<TypeParam>& typeParams, std::vector<Type*>& variables)
{
    for (auto& typeParam : typeParams)
    {
        resolveTypeParam(node, typeParam, variables);
    }
}

void SemanticAnalyzer::resolveTypeParam(AstNode* node, const TypeParam& typeParam, std::vector<Type*>& variables)
{
    auto& typeContext = _typeContexts.back();

    const std::string& typeParameter = typeParam.name;
    const std::vector<TypeName*>& constraints = typeParam.constraints;

    CHECK_UNDEFINED(typeParameter);
    CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter `{}` is already defined", typeParameter);
    Type* var = _typeTable->createTypeVariable(typeParameter, true);

    auto result = addConstraints(var, constraints);
    CHECK(result.first, result.second);

    typeContext.emplace(typeParameter, var);
    variables.push_back(var);
}

void SemanticAnalyzer::resolveWhereClause(AstNode* node, const std::vector<TypeParam>& typeParams)
{
    auto& typeContext = _typeContexts.back();

    for (auto& item : typeParams)
    {
        const std::string& typeParameter = item.name;
        const std::vector<TypeName*>& constraints = item.constraints;

        CHECK(typeParameter.size() == 1, "`{}` does not name a type parameter", typeParameter);
        CHECK(!constraints.empty(), "type parameter `{}` appears in a where clause unconstrained", typeParameter);

        auto i = typeContext.find(typeParameter);
        CHECK(i != typeContext.end(), "type parameter `{}` was not previously defined", typeParameter);

        Type* var = i->second;

        auto result = addConstraints(var, constraints, true);
        CHECK(result.first, result.second);
    }
}

void SemanticAnalyzer::resolveTypeNameWhere(AstNode* node, TypeName* typeName, const std::vector<TypeParam>& whereClause)
{
    auto& typeContext = _typeContexts.back();

    // Impose the constraints specified by the where clause
    std::vector<TypeVariable*> whereClauseVars;
    for (auto& item : whereClause)
    {
        const std::string& name = item.name;
        const std::vector<TypeName*>& constraints = item.constraints;

        CHECK(name.size() == 1, "`{}` does not name a type parameter", name);
        CHECK(!constraints.empty(), "type parameter `{}` appears in a where clause unconstrained", name);

        Type* var;

        auto i = typeContext.find(name);
        if (i == typeContext.end())
        {
            var = _typeTable->createTypeVariable(name, true);
            whereClauseVars.push_back(var->get<TypeVariable>());
            typeContext.emplace(name, var);
        }
        else
        {
            var = i->second;
        }

        auto result = addConstraints(var, constraints, true);
        CHECK(result.first, result.second);
    }

    // Resolve the type name in the context given by the where clause
    resolveTypeName(typeName, true);

    // Make sure that every variable constrained by the where clause actually
    // occurs in the type name
    for (TypeVariable* var : whereClauseVars)
    {
        CHECK(occurs(var, typeName->type), "type parameter `{}` was not previously defined", var->name());
    }
}



//// Visitor functions /////////////////////////////////////////////////////////

void SemanticAnalyzer::visit(ProgramNode* node)
{
	injectSymbols();

	//// Recurse down into children
	AstVisitor::visit(node);

    for (auto& child : node->children)
    {
        unify(child->type, _typeTable->Unit, child);
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
    CHECK(typeName.size() > 1, "type names must contain at least 2 characters");

    resolveTypeName(node->underlying);

    // Insert the alias into the type table
    _symbolTable->createTypeSymbol(typeName, node, node->underlying->type);

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(FunctionDefNode* node)
{
	// Functions cannot be declared inside of another function
    CHECK(!_enclosingFunction, "functions cannot be nested");

	// The function name cannot have already been used as something else
	const std::string& name = node->name;
    CHECK_UNDEFINED(name);

    pushTypeContext();
    resolveTypeNameWhere(node, node->typeName, node->typeParams);

    Type* type = node->typeName->type;
    FunctionType* functionType = type->get<FunctionType>();
    node->functionType = functionType;

    assert(functionType->inputs().size() == node->params.size());

    const std::vector<Type*>& paramTypes = functionType->inputs();

	FunctionSymbol* symbol = _symbolTable->createFunctionSymbol(name, node, node);
    symbol->type = type;
	node->symbol = symbol;

	_symbolTable->pushScope();

	// Add symbols corresponding to the formal parameters to the
	// function's scope
	for (size_t i = 0; i < node->params.size(); ++i)
	{
		const std::string& param = node->params[i];

		VariableSymbol* paramSymbol = _symbolTable->createVariableSymbol(param, node, node, false);
		paramSymbol->isParam = true;
        paramSymbol->offset = i;
		paramSymbol->type = paramTypes[i];

        node->parameterSymbols.push_back(paramSymbol);
	}

	// Recurse
	_enclosingFunction = node;
	node->body->accept(this);
	_enclosingFunction = nullptr;

	_symbolTable->popScope();
    popTypeContext();

    node->type = _typeTable->Unit;

    if (!equals(functionType->output(), _typeTable->Unit))
    {
        CHECK(_returnChecker.checkFunction(node), "not every path through function returns a value");
    }
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

    // Create type variables for each type parameter
    TypeContext typeContext;
    for (auto& typeParameter : node->typeParams)
    {
        CHECK_UNDEFINED(typeParameter);
        CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter `{}` is already defined", typeParameter);

        Type* var = _typeTable->createTypeVariable(typeParameter, true);
        typeContext.emplace(typeParameter, var);
    }

    pushTypeContext(std::move(typeContext));
    resolveTypeName(node->typeName, true);
    popTypeContext();

    Type* functionType = node->typeName->type;
    assert(functionType->get<FunctionType>()->inputs().size() == node->params.size());

	FunctionSymbol* symbol = _symbolTable->createFunctionSymbol(name, node, nullptr);
    symbol->type = functionType;
	symbol->isExternal = true;
	node->symbol = symbol;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(VariableDefNode* node)
{
	// Do this first so that we can't have recursive definitions.
	node->rhs->accept(this);

	const std::string& target = node->target;
    if (target != "_")
    {
        CHECK_UNDEFINED_IN_SCOPE(target);

        bool global = _symbolTable->isTopScope();
    	VariableSymbol* symbol = _symbolTable->createVariableSymbol(target, node, _enclosingFunction, global);

        symbol->type = node->rhs->type;
    	node->symbol = symbol;
    }
    else
    {
        node->symbol = nullptr;
    }

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(MatchNode* node)
{
    node->expr->accept(this);
    Type* type = node->expr->type;

    std::set<size_t> constructorTags;
    for (auto& arm : node->arms)
    {
        arm->matchType = type;
        arm->accept(this);

        if (!arm->constructorSymbol)
        {
            CHECK_AT(arm->location, !node->catchallArm, "cannot have more than one catch-all pattern");
            node->catchallArm = arm;
        }
        else
        {
            bool notDuplicate = (constructorTags.find(arm->constructorTag) == constructorTags.end());
            CHECK_AT(arm->location, notDuplicate, "cannot repeat constructors in match statement");
            constructorTags.insert(arm->constructorTag);
        }
    }

    CHECK(node->catchallArm || constructorTags.size() == type->valueConstructors().size(), "switch statement is not exhaustive");

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(MatchArm* node)
{
    const std::string& constructorName = node->constructor;

    // Catch-all pattern
    if (constructorName == "Else")
    {
        node->body->accept(this);
        unify(node->body->type, _typeTable->Unit, node->body);
        node->type = _typeTable->Unit;
        return;
    }

    Symbol* symbol = resolveSymbol(constructorName);
    CHECK(symbol, "constructor `{}` is not defined", constructorName);

    std::pair<size_t, ValueConstructor*> result = node->matchType->getValueConstructor(constructorName);
    CHECK(result.second, "type `{}` has no value constructor named `{}`", node->matchType->str(), constructorName);
    node->constructorTag = result.first;
    node->valueConstructor = result.second;

    ConstructorSymbol* constructorSymbol = dynamic_cast<ConstructorSymbol*>(symbol);
    assert(constructorSymbol);
    node->constructorSymbol = constructorSymbol;

    // A symbol with a capital letter should always be a constructor
    assert(constructorSymbol->kind == kFunction);
    assert(constructorSymbol->type->tag() == ttFunction);

    Type* instantiatedType = instantiate(constructorSymbol->type, node->typeAssignment);
    FunctionType* functionType = instantiatedType->get<FunctionType>();
    Type* constructedType = functionType->output();
    unify(constructedType, node->matchType, node);

    CHECK(functionType->inputs().size() == node->params.size(),
        "constructor pattern `{}` does not have the correct number of arguments", constructorName);

    _symbolTable->pushScope();

    // And create new variables for each of the members of the constructor
    for (size_t i = 0; i < node->params.size(); ++i)
    {
        const std::string& name = node->params[i];
        CHECK_UNDEFINED_IN_SCOPE(name);

        if (name != "_")
        {
            VariableSymbol* member = _symbolTable->createVariableSymbol(name, node, _enclosingFunction, false);
            member->type = functionType->inputs().at(i);
            node->symbols.push_back(member);
        }
        else
        {
            node->symbols.push_back(nullptr);
        }
    }

    // Visit body
    node->body->accept(this);
    unify(node->body->type, _typeTable->Unit, node->body);

    _symbolTable->popScope();

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(LetNode* node)
{
	AstVisitor::visit(node);

	const std::string& constructor = node->constructor;
	Symbol* symbol = resolveSymbol(constructor);
    CHECK(symbol, "constructor `{}` is not defined", constructor);

    ConstructorSymbol* constructorSymbol = dynamic_cast<ConstructorSymbol*>(symbol);
    CHECK(constructorSymbol, "`{}` is not a value constructor", constructor);
    node->constructorSymbol = constructorSymbol;

	// A symbol with a capital letter should always be a constructor
	assert(constructorSymbol->kind == kFunction);

	assert(constructorSymbol->type->tag() == ttFunction);
    Type* instantiatedType = instantiate(constructorSymbol->type, node->typeAssignment);
	FunctionType* functionType = instantiatedType->get<FunctionType>();
	Type* constructedType = functionType->output();

    // When used as a statement, let cannot fail to match
    if (!node->isExpression)
    {
        CHECK(constructedType->valueConstructors().size() == 1, "let statement pattern matching only applies to types with a single constructor");
    }

    CHECK(functionType->inputs().size() == node->params.size(), "constructor pattern `{}` does not have the correct number of arguments", constructor);

    node->valueConstructor = constructorSymbol->constructor;

    bool global = _symbolTable->isTopScope();

	// And create new variables for each of the members of the constructor
	for (size_t i = 0; i < node->params.size(); ++i)
	{
		const std::string& name = node->params[i];
        CHECK_UNDEFINED_IN_SCOPE(name);

        if (name != "_")
        {
    		VariableSymbol* member = _symbolTable->createVariableSymbol(name, node, _enclosingFunction, global);
    		member->type = functionType->inputs().at(i);
    		node->symbols.push_back(member);
        }
        else
        {
            node->symbols.push_back(nullptr);
        }
	}

	unify(node->body->type, functionType->output(), node);

    if (node->isExpression)
    {
        node->type = _typeTable->Bool;
    }
    else
    {
        node->type = _typeTable->Unit;
    }
}

void SemanticAnalyzer::visit(AssignNode* node)
{
    node->rhs->accept(this);

    LValueAnalyzer lvalueAnalyzer(this);
    node->lhs->accept(&lvalueAnalyzer);

    if (!lvalueAnalyzer.good())
    {
        semanticError(node->location, "left-hand side of assignment statement is not an lvalue");
        return;
    }

    unify(node->lhs->type, node->rhs->type, node->rhs);
    node->type = _typeTable->Unit;
}

void LValueAnalyzer::visit(NullaryNode* node)
{
    const std::string& name = node->name;

    Symbol* symbol = _mainAnalyzer->resolveSymbol(name);
    CHECK(symbol, "symbol `{}` is not defined in this scope. Did you mean to define it here?", name);

    if (symbol->kind != kVariable)
        return;

    node->symbol = symbol;
    node->type = symbol->type;
    node->kind = NullaryNode::VARIABLE;

    _good = true;
}

void LValueAnalyzer::visit(MemberAccessNode* node)
{
    node->accept(_mainAnalyzer);
    _good = true;
}

void LValueAnalyzer::visit(IndexNode* node)
{
    node->object->accept(_mainAnalyzer);
    node->index->accept(_mainAnalyzer);

    TraitSymbol* traitSymbol = dynamic_cast<TraitSymbol*>(_mainAnalyzer->resolveTypeSymbol("IndexSet"));
    assert(traitSymbol);

    TypeAssignment typeAssignment;
    Trait* trait = instantiate(traitSymbol->trait, typeAssignment);
    assert(trait);

    node->setMethod = traitSymbol->methods["set"];
    assert(node->setMethod);

    FunctionType* methodType = instantiate(node->setMethod->type, typeAssignment)->get<FunctionType>();
    assert(methodType);

    assert(methodType->inputs().size() == 3);
    unify(methodType->inputs()[0], node->object->type, node);
    unify(methodType->inputs()[1], node->index->type, node);
    node->type = methodType->inputs()[2];

    assert(equals(methodType->output(), _mainAnalyzer->_typeTable->Unit));

    _good = true;
}

void SemanticAnalyzer::visit(FunctionCallNode* node)
{
	const std::string& name = node->target;

    Symbol* symbol;
    if (!node->typeName) // Regular function call
    {
        symbol = resolveSymbol(name);
        CHECK(symbol, "function `{}` is not defined", name);
    }
    else // Static method
    {
        resolveTypeName(node->typeName);

        std::vector<MemberSymbol*> symbols;
        _symbolTable->resolveMemberSymbol(name, node->typeName->type, symbols);
        CHECK(!symbols.empty(), "no method named `{}` found for type `{}`", name, node->typeName->type->str());
        CHECK(symbols.size() < 2, "method call is ambiguous");

        CHECK(!symbols.front()->isMemberVar(), "`{}` is a member variable, not a method", name);
        symbol = symbols.front();
    }

    assert(symbol);

    Type* expectedType = instantiate(symbol->type, node->typeAssignment);
    CHECK(expectedType->tag() == ttFunction, "`{}` is not a function", name);

    FunctionType* functionType = expectedType->get<FunctionType>();
    CHECK(functionType->inputs().size() >= node->arguments.size(), "function called with too many arguments")
    CHECK(functionType->inputs().size() <= node->arguments.size(), "function called with too few arguments");

	std::vector<Type*> paramTypes;
    for (size_t i = 0; i < node->arguments.size(); ++i)
    {
        AstNode* argument = node->arguments[i];
        argument->accept(this);

        unify(argument->type, functionType->inputs()[i], argument);

        paramTypes.push_back(argument->type);
    }

	node->symbol = symbol;
    node->type = functionType->output();
}

void SemanticAnalyzer::visit(MethodCallNode* node)
{
    node->object->accept(this);
    Type* objectType = node->object->type;

    std::vector<MemberSymbol*> symbols;
    _symbolTable->resolveMemberSymbol(node->methodName, objectType, symbols);
    CHECK(!symbols.empty(), "no method named `{}` found for type `{}`", node->methodName, objectType->str());
    CHECK(symbols.size() < 2, "method call is ambiguous");

    CHECK(!symbols.front()->isMemberVar(), "`{}` is a member variable, not a method", node->methodName);
    Symbol* symbol = symbols.front();

    Type* expectedType = instantiate(symbol->type, node->typeAssignment);
    assert(expectedType->tag() == ttFunction);

    FunctionType* functionType = expectedType->get<FunctionType>();
    CHECK(functionType->inputs().size() >= node->arguments.size() + 1, "method called with too many arguments")
    CHECK(functionType->inputs().size() <= node->arguments.size() + 1, "method called with too few arguments");

    unify(objectType, functionType->inputs()[0], node->object);

    std::vector<Type*> paramTypes = {objectType};
    for (size_t i = 0; i < node->arguments.size(); ++i)
    {
        AstNode* argument = node->arguments[i];
        argument->accept(this);

        unify(argument->type, functionType->inputs()[i + 1], argument);

        paramTypes.push_back(argument->type);
    }

    node->symbol = symbol;
    node->type = functionType->output();
}

void SemanticAnalyzer::visit(BinopNode* node)
{
    node->lhs->accept(this);
    node->rhs->accept(this);
    unify(node->lhs->type, node->rhs->type, node);

    // Arithmetic on numerical types are built-in
    if (!isSubtype(node->lhs->type, _typeTable->Num))
    {
        std::string traitName;
        std::string methodName;
        switch (node->op)
        {
            case BinopNode::kAdd:
                traitName = "Add";
                methodName = "add";
                break;

            case BinopNode::kSub:
                traitName = "Sub";
                methodName = "sub";
                break;

            case BinopNode::kMul:
                traitName = "Mul";
                methodName = "mul";
                break;

            case BinopNode::kDiv:
                traitName = "kDiv";
                methodName = "div";
                break;

            case BinopNode::kRem:
                traitName = "kRem";
                methodName = "rem";
                break;
        }

        TraitSymbol* traitSymbol = dynamic_cast<TraitSymbol*>(resolveTypeSymbol(traitName));
        assert(traitSymbol);

        Trait* trait = traitSymbol->trait;

        unify(node->lhs->type, trait, node->lhs);
        unify(node->rhs->type, trait, node->rhs);

        node->method = traitSymbol->methods[methodName];
        assert(node->method);
    }

    node->type = node->lhs->type;
}

void SemanticAnalyzer::visit(CastNode* node)
{
    node->lhs->accept(this);
    resolveTypeName(node->typeName);

    if (node->typeName->type->isVariable())
    {
        semanticError(node->location, "Cannot cast to generic type");
    }

    node->type = node->typeName->type;

    Type* srcType = node->lhs->type;
    Type* destType = node->type;

    // Only supported casts
    if (srcType->equals(destType))
        return;

    if (isSubtype(srcType, _typeTable->Num) && isSubtype(destType, _typeTable->Num))
        return;

    semanticError(node->location, "Cannot cast from type {} to {}", srcType->str(), destType->str());
}

void SemanticAnalyzer::visit(NullaryNode* node)
{
	const std::string& name = node->name;

	Symbol* symbol = resolveSymbol(name);
    CHECK(symbol, "symbol `{}` is not defined in this scope", name);
    CHECK(symbol->kind == kVariable || symbol->kind == kFunction, "symbol `{}` is not a variable or a function", name);

	if (symbol->kind == kVariable)
	{
		node->symbol = symbol;
        node->type = symbol->type;
        node->kind = NullaryNode::VARIABLE;
	}
	else /* symbol->kind == kFunction */
	{
		node->symbol = symbol;
        Type* functionType = instantiate(symbol->type, node->typeAssignment);

        FunctionSymbol* functionSymbol = dynamic_cast<FunctionSymbol*>(symbol);
        if (functionSymbol->isConstructor && functionType->get<FunctionType>()->inputs().empty())
        {
            node->type = functionType->get<FunctionType>()->output();
            node->kind = NullaryNode::FUNC_CALL;
        }
        else
        {
            CHECK(!functionSymbol->isExternal, "Cannot put external function `{}` into a closure", name);

            node->type = functionType;
            node->kind = NullaryNode::CLOSURE;
        }
	}
}

void SemanticAnalyzer::visit(ComparisonNode* node)
{
    node->lhs->accept(this);
    node->rhs->accept(this);
    unify(node->lhs->type, node->rhs->type, node);

    if (!isSubtype(node->lhs->type, _typeTable->Num))
    {
        TraitSymbol* traitSymbol;
        if (node->op == ComparisonNode::kEqual || node->op == ComparisonNode::kNotEqual)
        {
            traitSymbol = dynamic_cast<TraitSymbol*>(resolveTypeSymbol("Eq"));
        }
        else
        {
            traitSymbol = dynamic_cast<TraitSymbol*>(resolveTypeSymbol("PartialOrd"));
        }

        assert(traitSymbol);

        Trait* trait = traitSymbol->trait;

        unify(node->lhs->type, trait, node->lhs);
        unify(node->rhs->type, trait, node->rhs);

        switch (node->op)
        {
            case ComparisonNode::kEqual:
                node->method = traitSymbol->methods["eq"];
                break;

            case ComparisonNode::kNotEqual:
                node->method = traitSymbol->methods["ne"];
                break;

            case ComparisonNode::kLess:
                node->method = traitSymbol->methods["lt"];
                break;

            case ComparisonNode::kLessOrEqual:
                node->method = traitSymbol->methods["le"];
                break;

            case ComparisonNode::kGreater:
                node->method = traitSymbol->methods["gt"];
                break;

            case ComparisonNode::kGreaterOrEqual:
                node->method = traitSymbol->methods["ge"];
                break;
        }

        assert(node->method);
    }

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
    for (auto& child : node->children)
    {
        child->accept(this);
        unify(child->type, _typeTable->Unit, child);
    }

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(IfElseNode* node)
{
    _symbolTable->pushScope();

    node->condition->accept(this);
    unify(node->condition->type, _typeTable->Bool, node);

    node->body->accept(this);
    unify(node->body->type, _typeTable->Unit, node->body);

    _symbolTable->popScope();

    if (node->elseBody)
    {
        _symbolTable->pushScope();

        node->elseBody->accept(this);
        unify(node->elseBody->type, _typeTable->Unit, node->elseBody);

        _symbolTable->popScope();
    }

    node->type = node->body->type;
}

void SemanticAnalyzer::visit(AssertNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, _typeTable->Bool, node);

    // HACK: Give the code generator access to these symbols
    node->panicSymbol = dynamic_cast<FunctionSymbol*>(resolveSymbol("panic"));
    assert(node->panicSymbol);

    node->type = _typeTable->Unit;
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
    _symbolTable->pushScope();

    node->body->accept(this);

    _symbolTable->popScope();
    _enclosingLoop = outerLoop;

    unify(node->body->type, _typeTable->Unit, node);
}

void SemanticAnalyzer::visit(ForNode* node)
{
    node->iterableExpression->accept(this);

    TypeAssignment typeAssignment;
    node->iterableSymbol = dynamic_cast<TraitSymbol*>(resolveTypeSymbol("Iterable"));
    Trait* Iterable = instantiate(node->iterableSymbol->trait, typeAssignment);
    unify(node->iterableExpression->type, Iterable, node->iterableExpression);

    node->iter = node->iterableSymbol->methods.at("iter");
    Type* varType = Iterable->parameters()[0];

    TraitSymbol* iteratorSymbol = dynamic_cast<TraitSymbol*>(resolveTypeSymbol("Iterator"));
    Trait* Iterator = iteratorSymbol->trait->instantiate({varType});

    node->next = iteratorSymbol->methods.at("next");

    Type* Option = dynamic_cast<TypeSymbol*>(resolveTypeSymbol("Option"))->type;
    node->optionType = Option->get<ConstructedType>()->instantiate({varType});

    // Save the current inner-most loop so that we can restore it after
    // visiting the children of this loop.
    LoopNode* outerLoop = _enclosingLoop;
    _enclosingLoop = node;
    _symbolTable->pushScope();

    node->symbol = _symbolTable->createVariableSymbol(node->varName, node, _enclosingFunction, false);
    node->symbol->type = varType;

    node->body->accept(this);
    unify(node->body->type, _typeTable->Unit, node);

    _symbolTable->popScope();
    _enclosingLoop = outerLoop;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(ForeverNode* node)
{
    // Save the current inner-most loop so that we can restore it after
    // visiting the children of this loop.
    LoopNode* outerLoop = _enclosingLoop;

    _enclosingLoop = node;
    _symbolTable->pushScope();

    node->body->accept(this);

    _symbolTable->popScope();
    _enclosingLoop = outerLoop;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(BreakNode* node)
{
    CHECK(_enclosingLoop, "break statement must be within a loop");

    node->loop = _enclosingLoop;
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(ContinueNode* node)
{
    CHECK(_enclosingLoop, "continue statement must be within a loop");

    node->loop = _enclosingLoop;
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(IntNode* node)
{
    if (node->suffix == "u")
    {
        node->type = _typeTable->UInt;
    }
    else if (node->suffix == "i")
    {
        node->type = _typeTable->Int;
    }
    else if (node->suffix == "u8")
    {
        node->type = _typeTable->UInt8;
    }
    else
    {
        assert(node->suffix.empty());

        // The signedness of integers without a suffix is inferred. This will
        // be checked in the second pass
        node->type = _typeTable->createTypeVariable();
        node->type->get<TypeVariable>()->addConstraint(_typeTable->Num);
    }
}

void SemanticAnalyzer::visit(BoolNode* node)
{
    node->type = _typeTable->Bool;
}

void SemanticAnalyzer::visit(StringLiteralNode* node)
{
    Type* String = resolveTypeSymbol("String")->type;
    node->type = String;

    std::string name = "__staticString" + std::to_string(node->counter);
    VariableSymbol* symbol = _symbolTable->createVariableSymbol(name, node, nullptr, true);
    symbol->isStatic = true;
    symbol->contents = node->content;
    node->symbol = symbol;
}

void SemanticAnalyzer::visit(ReturnNode* node)
{
    CHECK(_enclosingFunction, "Cannot return from top level");

    Type* type = _enclosingFunction->symbol->type;
    FunctionType* functionType = type->get<FunctionType>();
    assert(functionType);

    if (node->expression)
    {
        node->expression->accept(this);
        unify(node->expression->type, functionType->output(), node);
    }
    else
    {
        unify(_typeTable->Unit, functionType->output(), node);
    }

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(DataDeclaration* node)
{
    // Data declarations cannot be local
    CHECK_TOP_LEVEL("data declaration");

    // The data type name cannot have already been used for something
    const std::string& name = node->name;
    CHECK_UNDEFINED(name);
    CHECK(name.size() > 1, "type names must contain at least two characters");

    // Actually create the type
    if (node->typeParameters.empty())
    {
        Type* newType = _typeTable->createBaseType(node->name);
        _symbolTable->createTypeSymbol(name, node, newType);

        for (size_t i = 0; i < node->constructorSpecs.size(); ++i)
        {
            auto& spec = node->constructorSpecs[i];
            spec->constructorTag = i;
            spec->resultType = newType;
            spec->accept(this);

            node->valueConstructors.push_back(spec->valueConstructor);
            node->constructorSymbols.push_back(spec->symbol);
        }
    }
    else
    {
        // Create type variables for each type parameter
        std::vector<Type*> variables;
        TypeContext typeContext;
        for (auto& typeParameter : node->typeParameters)
        {
            CHECK_UNDEFINED(typeParameter);
            CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter `{}` is already defined", typeParameter);

            Type* var = _typeTable->createTypeVariable(typeParameter, true);
            variables.push_back(var);

            typeContext.emplace(typeParameter, var);
        }

        Type* newType = _typeTable->createConstructedType(name, std::move(variables));
        _symbolTable->createTypeSymbol(name, node, newType);

        pushTypeContext(std::move(typeContext));
        for (size_t i = 0; i < node->constructorSpecs.size(); ++i)
        {
            auto& spec = node->constructorSpecs[i];
            spec->constructorTag = i;
            spec->resultType = newType;
            spec->accept(this);

            node->valueConstructors.push_back(spec->valueConstructor);
            node->constructorSymbols.push_back(spec->symbol);
        }
        popTypeContext();
    }

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(ConstructorSpec* node)
{
    CHECK_UNDEFINED_SYMBOL(node->name);

    // Resolve member types
    std::vector<MemberVarSymbol*> memberSymbols;
    for (size_t i = 0; i < node->members.size(); ++i)
    {
        auto& member = node->members[i];

        resolveTypeName(member);
        node->memberTypes.push_back(member->type);

        MemberVarSymbol* memberSymbol = _symbolTable->createMemberVarSymbol("_", node, nullptr, node->resultType, i);
        memberSymbol->type = _typeTable->createFunctionType({node->resultType}, member->type);
        memberSymbols.push_back(memberSymbol);
    }

    ValueConstructor* valueConstructor = _typeTable->createValueConstructor(node->name, node->constructorTag, node->memberTypes);
    node->valueConstructor = valueConstructor;
    node->resultType->addValueConstructor(valueConstructor);

    // Create a symbol for the constructor
    ConstructorSymbol* symbol = _symbolTable->createConstructorSymbol(node->name, node, valueConstructor, memberSymbols);
    symbol->type = _typeTable->createFunctionType(node->memberTypes, node->resultType);
    node->symbol = symbol;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(StructDefNode* node)
{
    // Struct definitions cannot be local
    CHECK_TOP_LEVEL("struct declaration");

    // The type name cannot have already been used for something
    const std::string& typeName = node->name;
    CHECK_UNDEFINED(typeName);
    CHECK(typeName.size() > 1, "type names must contain at least two characters");

    CHECK(!node->members.empty(), "structs cannot be empty");

    // TODO: Refactor these two cases (and maybe DataDeclaration as well)
    if (node->typeParams.empty())
    {
        AstVisitor::visit(node);

        Type* newType = _typeTable->createBaseType(node->name);
        _symbolTable->createTypeSymbol(typeName, node, newType);

        std::vector<std::string> memberNames;
        std::vector<Type*> memberTypes;
        std::vector<MemberVarSymbol*> memberSymbols;
        std::unordered_set<std::string> alreadyUsed;
        for (size_t i = 0; i < node->members.size(); ++i)
        {
            auto& member = node->members[i];

            // Make sure there are no repeated member names
            CHECK(alreadyUsed.find(member->name) == alreadyUsed.end(), "type `{}` already has a member named `{}`", node->name, member->name);
            alreadyUsed.insert(member->name);

            memberTypes.push_back(member->memberType);
            memberNames.push_back(member->name);

            MemberVarSymbol* memberSymbol = _symbolTable->createMemberVarSymbol(member->name, node, nullptr, newType, i);
            memberSymbol->type = _typeTable->createFunctionType({newType}, member->memberType);
            memberSymbols.push_back(memberSymbol);
        }

        ValueConstructor* valueConstructor = _typeTable->createValueConstructor(typeName, 0, memberTypes, memberNames);
        node->valueConstructor = valueConstructor;
        newType->addValueConstructor(valueConstructor);

        // Create a symbol for the constructor
        ConstructorSymbol* symbol = _symbolTable->createConstructorSymbol(typeName, node, valueConstructor, memberSymbols);
        symbol->type = _typeTable->createFunctionType(memberTypes, newType);
        node->constructorSymbol = symbol;

        node->structType = newType;
    }
    else
    {
        pushTypeContext();

        // Create type variables for each type parameter
        std::vector<Type*> variables;
        resolveTypeParams(node, node->typeParams, variables);
        resolveWhereClause(node, node->whereClause);

        Type* newType = _typeTable->createConstructedType(typeName, std::move(variables));
        _symbolTable->createTypeSymbol(typeName, node, newType);

        std::vector<std::string> memberNames;
        std::vector<Type*> memberTypes;
        std::vector<MemberVarSymbol*> memberSymbols;
        std::unordered_set<std::string> alreadyUsed;
        for (size_t i = 0; i < node->members.size(); ++i)
        {
            MemberDefNode* member = node->members[i];

            // Make sure there are no repeated member names
            CHECK(alreadyUsed.find(member->name) == alreadyUsed.end(), "type `{}` already has a member named `{}`", node->name, member->name);
            alreadyUsed.insert(member->name);

            member->accept(this);

            memberTypes.push_back(member->memberType);
            memberNames.push_back(member->name);

            MemberVarSymbol* memberSymbol = _symbolTable->createMemberVarSymbol(member->name, node, nullptr, newType, i);
            memberSymbol->type = _typeTable->createFunctionType({newType}, member->memberType);
            memberSymbols.push_back(memberSymbol);
        }

        popTypeContext();

        ValueConstructor* valueConstructor = _typeTable->createValueConstructor(typeName, 0, memberTypes, memberNames);
        node->valueConstructor = valueConstructor;

        // Create a symbol for the constructor
        ConstructorSymbol* constructorSymbol = _symbolTable->createConstructorSymbol(typeName, node, valueConstructor, memberSymbols);
        constructorSymbol->type = _typeTable->createFunctionType(memberTypes, newType);
        node->constructorSymbol = constructorSymbol;

        node->structType = newType;
    }

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
    node->object->accept(this);
    Type* objectType = node->object->type;

    std::vector<MemberSymbol*> symbols;
    _symbolTable->resolveMemberSymbol(node->memberName, objectType, symbols);
    CHECK(!symbols.empty(), "no member named `{}` found for type `{}`", node->memberName, objectType->str());
    assert(symbols.size() < 2);

    CHECK(symbols.front()->isMemberVar(), "`{}` is a method, not a member variable", node->memberName);
    MemberVarSymbol* symbol = dynamic_cast<MemberVarSymbol*>(symbols.front());
    node->symbol = symbol;

    // Member var types are stored in the symbol table in the form |ObjectType| -> MemberType
    // so that the same matching machinery can be used for member variables as for methods
    FunctionType* functionType = instantiate(symbol->type, node->typeAssignment)->get<FunctionType>();
    assert(functionType->inputs().size() == 1);
    unify(functionType->inputs()[0], objectType, node);

    node->symbol = symbol;
    node->type = functionType->output();
    node->constructorSymbol = symbol->constructorSymbol;
    node->memberIndex = symbol->index;
}

void SemanticAnalyzer::visit(IndexNode* node)
{
    node->object->accept(this);
    node->index->accept(this);

    TraitSymbol* traitSymbol = dynamic_cast<TraitSymbol*>(resolveTypeSymbol("Index"));
    assert(traitSymbol);

    TypeAssignment typeAssignment;
    Trait* trait = instantiate(traitSymbol->trait, typeAssignment);
    assert(trait);

    node->atMethod = traitSymbol->methods["at"];
    assert(node->atMethod);

    FunctionType* methodType = instantiate(node->atMethod->type, typeAssignment)->get<FunctionType>();
    assert(methodType);

    assert(methodType->inputs().size() == 2);
    unify(methodType->inputs()[0], node->object->type, node);
    unify(methodType->inputs()[1], node->index->type, node);

    node->type = methodType->output();
}

void SemanticAnalyzer::visit(ImplNode* node)
{
    CHECK_TOP_LEVEL("method implementation block");

    assert(!_enclosingImplNode);
    _enclosingImplNode = node;

    _symbolTable->pushScope();
    pushTypeContext();

    TraitSymbol* traitSymbol = nullptr;
    std::vector<Type*> traitParameters;
    if (node->traitName)
    {
        traitSymbol = resolveTrait(node->traitName, traitParameters, true);

        CHECK(traitSymbol->node, "can't create new instance for built-in trait `{}`", node->traitName->name);
    }

    resolveTypeNameWhere(node, node->typeName, node->typeParams);

    // Don't allow extraneous type variables, like this:
    // impl<T> Test. This also implies that if this is a trait impl block, then
    // the trait is no more generic than the object type. In other words, for
    // each concrete object type, there is a unique concrete trait
    for (auto& item : _typeContexts.back())
    {
        CHECK(occurs(item.second->get<TypeVariable>(), node->typeName->type),
            "type variable `{}` doesn't occur in type `{}`",
            item.first,
            node->typeName->type->str());
    }

    node->typeContext = _typeContexts.back();

    if (traitSymbol)
    {
        Type* overlapping = findOverlappingInstance(traitSymbol->trait, node->typeName->type);
        if (overlapping)
        {
            std::stringstream ss;
            ss << "trait `" << traitSymbol->name
               << "` already has an instance which would overlap with `"
               << node->typeName->type->str() << "`" << std::endl;
            ss << "Previous impl for type `" << overlapping->str() << "` at "
               << instanceLocation(traitSymbol, overlapping);
            semanticError(node->location, ss.str());
        }
    }

    // First pass
    // methods: check prototype, and create symbol
    // associated types: add to type context
    std::unordered_map<std::string, MethodSymbol*> methods;
    TypeContext associatedTypes;
    for (auto& member : node->members)
    {
        member->accept(this);

        if (MethodDefNode* method = dynamic_cast<MethodDefNode*>(member))
        {
            methods[method->name] = dynamic_cast<MethodSymbol*>(method->symbol);
        }
        else if (TypeAliasNode* typeAlias = dynamic_cast<TypeAliasNode*>(member))
        {
            // Do something
            associatedTypes[typeAlias->name] = typeAlias->underlying->type;
        }
        else
        {
            assert(false);
        }
    }

    // If a trait method block, then check that we actually have implementations
    // for each trait method
    if (traitSymbol)
    {
        TypeAssignment traitSub;
        traitSub[traitSymbol->traitVar->get<TypeVariable>()] = node->typeName->type;

        for (auto& item : traitSymbol->associatedTypes)
        {
            std::string name = item.first;
            Type* variable = item.second;

            auto i = associatedTypes.find(name);
            CHECK(i != associatedTypes.end(), "no definition was given for associated type `{}` in trait `{}`", name, traitSymbol->name);

            Type* type = instantiate(variable, traitSub);
            unify(type, i->second, node);
        }

        for (auto& item : associatedTypes)
        {
            CHECK(traitSymbol->associatedTypes.find(item.first) != traitSymbol->associatedTypes.end(), "associated type `{}` is not a member of trait `{}`", item.first, traitSymbol->name);
        }

        for (auto& item : traitSymbol->methods)
        {
            std::string name = item.first;
            Type* type = instantiate(item.second->type, traitSub);

            auto i = methods.find(name);
            CHECK(i != methods.end(), "no implementation was given for method `{}` in trait `{}`", name, traitSymbol->name);

            unify(type, i->second->type, i->second->node);
        }

        // Make sure there aren't any extra methods as well
        for (auto& item : methods)
        {
            CHECK(traitSymbol->methods.find(item.first) != traitSymbol->methods.end(), "method `{}` is not a member of trait `{}`", item.first, traitSymbol->name);
        }

        traitSymbol->trait->addInstance(node->typeName->type, traitParameters);
        traitSymbol->addInstance(node->typeName->type, node, std::move(methods), std::move(associatedTypes));
    }

    // Third pass: check method bodies
    for (auto& member: node->members)
    {
        // Skip type aliases
        if (!dynamic_cast<MethodDefNode*>(member))
            continue;

        member->accept(this);
    }

    popTypeContext();
    _symbolTable->popScope();
    _enclosingImplNode = nullptr;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(MethodDefNode* node)
{
    if (!node->firstPassFinished)
    {
        // Functions cannot be declared inside of another function
        CHECK(!_enclosingFunction, "methods cannot be nested");
        CHECK(_enclosingImplNode, "methods can only appear inside impl blocks");

        Type* Self = _enclosingImplNode->typeName->type;
        TypeContext typeContext;
        typeContext["Self"] = Self;

        pushTypeContext(std::move(typeContext));
        resolveTypeNameWhere(node, node->typeName, node->typeParams);

        std::vector<MemberSymbol*> symbols;
        _symbolTable->resolveMemberSymbol(node->name, Self, symbols);
        CHECK(symbols.empty(), "type `{}` already has a method or member named `{}`", Self->str(), node->name);

        Type* type = node->typeName->type;
        FunctionType* functionType = type->get<FunctionType>();
        node->functionType = functionType;

        assert(functionType->inputs().size() == node->params.size());

        const std::vector<Type*>& paramTypes = functionType->inputs();

        MethodSymbol* symbol = _symbolTable->createMethodSymbol(node->name, node, Self);
        symbol->type = type;
        node->symbol = symbol;

        node->firstPassFinished = true;
    }
    else
    {
        FunctionType* functionType = node->functionType;
        const std::vector<Type*>& paramTypes = functionType->inputs();

        _symbolTable->pushScope();

        // Add symbols corresponding to the formal parameters to the
        // function's scope
        for (size_t i = 0; i < node->params.size(); ++i)
        {
            const std::string& param = node->params[i];

            VariableSymbol* paramSymbol = _symbolTable->createVariableSymbol(param, node, node, false);
            paramSymbol->isParam = true;
            paramSymbol->offset = i;
            paramSymbol->type = paramTypes[i];

            node->parameterSymbols.push_back(paramSymbol);
        }

        // Recurse
        _enclosingFunction = node;
        node->body->accept(this);
        _enclosingFunction = nullptr;

        _symbolTable->popScope();
        popTypeContext();

        node->type = _typeTable->Unit;
    }

    if (!equals(node->functionType->output(), _typeTable->Unit))
    {
        CHECK(_returnChecker.checkMethod(node), "not every path through method returns a value");
    }
}

void SemanticAnalyzer::visit(TraitDefNode* node)
{
    CHECK_TOP_LEVEL("trait definition");

    // The type name cannot have already been used for something
    const std::string& traitName = node->name;
    CHECK_UNDEFINED(traitName);

    // Create type variables for each type parameter
    TypeContext typeContext;
    std::vector<Type*> typeParameters;
    for (const std::string& typeParameter : node->typeParams)
    {
        CHECK_UNDEFINED(typeParameter);
        CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter `{}` is already defined", typeParameter);
        Type* var = _typeTable->createTypeVariable(typeParameter, true);

        typeContext.emplace(typeParameter, var);
        typeParameters.push_back(var);
    }

    std::vector<Type*> paramsCopy = typeParameters;
    Trait* trait = _typeTable->createTrait(traitName, std::move(paramsCopy));
    Type* traitVar = _typeTable->createTypeVariable("Self", true);
    traitVar->get<TypeVariable>()->addConstraint(trait);
    node->traitSymbol = _symbolTable->createTraitSymbol(traitName, node, trait, traitVar, std::move(typeParameters));

    _enclosingTraitDef = node;
    pushTypeContext(std::move(typeContext));
    for (auto& member : node->members)
    {
        member->accept(this);
    }
    popTypeContext();
    _enclosingTraitDef = nullptr;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(TraitMethodNode* node)
{
    // Functions cannot be declared inside of another function
    CHECK(!_enclosingFunction, "methods cannot be nested");
    assert(_enclosingTraitDef);

    TraitSymbol* traitSymbol = _enclosingTraitDef->traitSymbol;
    auto& methods = traitSymbol->methods;

    std::string name = node->name;
    CHECK(name != "_", "trait methods cannot be unnamed");
    CHECK(methods.find(name) == methods.end(), "trait `{}` already has a method named `{}`", traitSymbol->name, name);

    // TODO: Generic trait methods

    TypeContext typeContext;
    typeContext["Self"] = traitSymbol->traitVar;

    pushTypeContext(std::move(typeContext));
    resolveTypeName(node->typeName);
    popTypeContext();

    Type* type = node->typeName->type;
    FunctionType* functionType = type->get<FunctionType>();

    const std::vector<Type*>& paramTypes = functionType->inputs();

    TraitMethodSymbol* symbol = _symbolTable->createTraitMethodSymbol(node->name, node, traitSymbol);
    symbol->type = type;
    methods[name] = symbol;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(AssociatedTypeNode* node)
{
    assert(_enclosingTraitDef);

    TraitSymbol* traitSymbol = _enclosingTraitDef->traitSymbol;
    auto& associatedTypes = traitSymbol->associatedTypes;

    std::string name = node->typeParam.name;
    CHECK(name != "_", "associated types cannot be unnamed");
    CHECK(associatedTypes.find(name) == associatedTypes.end(), "trait `{}` already has an associated type named `{}`", traitSymbol->name, name);

    std::vector<Type*> variables;
    resolveTypeParam(node, node->typeParam, variables);

    associatedTypes[name] = variables[0];

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(PassNode* node)
{
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::checkTraitCoherence()
{
    std::vector<Trait*> traits = _typeTable->traits();

    for (Trait* trait : traits)
    {
        auto& instances = trait->instances();
        size_t n = instances.size();

        for (size_t i = 0; i + 1 < n; ++i)
        {
            for (size_t j = i + 1; j < n; ++j)
            {
                Type* instance1 = instances[i].type;
                Type* instance2 = instances[j].type;

                if (overlap(instance1, instance2))
                {
                    std::stringstream ss;

                    ss << "found overlapping instances for trait `" << trait->name() << "`" << std::endl;

                    Symbol* symbol = resolveTypeSymbol(trait->name());
                    assert(symbol);

                    TraitSymbol* traitSymbol = dynamic_cast<TraitSymbol*>(symbol);
                    assert(traitSymbol);

                    ss << "Impl for `" << instance1->str() << "` at " << instanceLocation(traitSymbol, instance1) << std::endl;
                    ss << "Impl for `" << instance2->str() << "` at " << instanceLocation(traitSymbol, instance2);

                    throw SemanticError(ss.str());
                }
            }
        }
    }
}
