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
            if (var->quantified())
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
		_root->accept(this);
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

    SemanticPass2 pass2(_context);
    return pass2.analyze();
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
    //// Built-in types ////////////////////////////////////////////////////////
    _symbolTable->createTypeSymbol("Int", _root, _typeTable->Int);
    _symbolTable->createTypeSymbol("UInt", _root, _typeTable->UInt);
    _symbolTable->createTypeSymbol("UInt8", _root, _typeTable->UInt8);

    Type* Self = _typeTable->createTypeVariable("Self", true);
    _symbolTable->createTraitSymbol("Num", _root, _typeTable->Num, Self);

    _symbolTable->createTypeSymbol("Bool", _root, _typeTable->Bool);
    _symbolTable->createTypeSymbol("Unit", _root, _typeTable->Unit);
    _symbolTable->createTypeSymbol("String", _root, _typeTable->String);

    _symbolTable->createTypeSymbol("Function", _root, _typeTable->Function);
    _symbolTable->createTypeSymbol("Array", _root, _typeTable->Array);


	//// Create symbols for built-in functions
    FunctionSymbol* notFn = createBuiltin("not");
	notFn->type = _typeTable->createFunctionType({_typeTable->Bool}, _typeTable->Bool);


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

void SemanticAnalyzer::resolveBaseType(TypeName* typeName)
{
    const std::string& name = typeName->name;

    Type* contextMatch = findInContext(name);
    if (contextMatch)
    {
        typeName->type = contextMatch;
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

void SemanticAnalyzer::resolveTypeName(TypeName* typeName)
{
    if (typeName->parameters.empty())
    {
        resolveBaseType(typeName);
    }
    else
    {
        std::vector<Type*> typeParameters;
        for (auto& parameter : typeName->parameters)
        {
            resolveTypeName(parameter);
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
                    CHECK_AT(typeName->location,
                             isSubtype(value, constraint),
                             "`{}` is not an instance of trait `{}`",
                             value->str(),
                             constraint->str());
                }

                typeMapping[variable] = value;
            }

            typeName->type = instantiate(type, typeMapping);
        }
    }
}

TraitSymbol* SemanticAnalyzer::resolveTrait(TypeName* traitName, std::vector<Type*>& traitParams)
{
    Symbol* symbol = resolveTypeSymbol(traitName->name);
    CHECK_AT(traitName->location, symbol->kind == kTrait, "`{}` is not a trait", traitName->name);

    TraitSymbol* traitSymbol = dynamic_cast<TraitSymbol*>(symbol);
    assert(traitSymbol);

    CHECK_AT(traitName->location, traitName->parameters.size() <= traitSymbol->typeParameters.size(), "`{}` has too many parameters", traitName->str());
    CHECK_AT(traitName->location, traitName->parameters.size() >= traitSymbol->typeParameters.size(), "`{}` has too few parameters", traitName->str());

    for (auto& traitParam : traitName->parameters)
    {
        resolveTypeName(traitParam);
        traitParams.push_back(traitParam->type);
    }

    return traitSymbol;
}

void SemanticAnalyzer::addConstraints(TypeVariable* var, const std::vector<TypeName*>& constraints)
{
    for (TypeName* constraint : constraints)
    {
        std::vector<Type*> traitParams;
        TraitSymbol* constraintSymbol = resolveTrait(constraint, traitParams);

        var->addConstraint(constraintSymbol->trait->instantiate(std::move(traitParams)));
    }
}

void SemanticAnalyzer::resolveTypeParams(AstNode* node, const std::vector<TypeParam>& typeParams, std::unordered_map<std::string, Type*>& typeContext)
{
    std::vector<Type*> variables;
    resolveTypeParams(node, typeParams, typeContext, variables);
}

void SemanticAnalyzer::resolveTypeParams(AstNode* node, const std::vector<TypeParam>& typeParams, std::unordered_map<std::string, Type*>& typeContext, std::vector<Type*>& variables)
{
    for (auto& item : typeParams)
    {
        const std::string& typeParameter = item.name;
        const std::vector<TypeName*>& constraints = item.constraints;

        CHECK_UNDEFINED(typeParameter);
        CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter `{}` is already defined", typeParameter);
        Type* var = _typeTable->createTypeVariable(typeParameter, true);

        addConstraints(var->get<TypeVariable>(), constraints);

        typeContext.emplace(typeParameter, var);
        variables.push_back(var);
    }
}


//// Visitor functions /////////////////////////////////////////////////////////

void SemanticAnalyzer::visit(ProgramNode* node)
{
    _symbolTable->pushScope();
	injectSymbols();

	//// Recurse down into children
	AstVisitor::visit(node);

    for (auto& child : node->children)
    {
        unify(child->type, _typeTable->Unit, child);
    }

    _symbolTable->popScope();

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

    // Create type variables for each type parameter
    std::unordered_map<std::string, Type*> typeContext;
    resolveTypeParams(node, node->typeParams, typeContext);

    _typeContexts.push_back(typeContext);
    resolveTypeName(node->typeName);

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
    _typeContexts.pop_back();

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

    // Create type variables for each type parameter
    std::unordered_map<std::string, Type*> typeContext;
    for (auto& typeParameter : node->typeParams)
    {
        CHECK_UNDEFINED(typeParameter);
        CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter `{}` is already defined", typeParameter);

        Type* var = _typeTable->createTypeVariable(typeParameter, true);
        typeContext.emplace(typeParameter, var);
    }

    _typeContexts.push_back(typeContext);
    resolveTypeName(node->typeName);
    _typeContexts.pop_back();

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

        bool notDuplicate = (constructorTags.find(arm->constructorTag) == constructorTags.end());
        CHECK_AT(arm->location, notDuplicate, "cannot repeat constructors in match statement");
        constructorTags.insert(arm->constructorTag);
    }

    CHECK(constructorTags.size() == type->valueConstructors().size(), "switch statement is not exhaustive");

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(MatchArm* node)
{
    const std::string& constructorName = node->constructor;
    std::pair<size_t, ValueConstructor*> result = node->matchType->getValueConstructor(constructorName);
    CHECK(result.second, "type `{}` has no value constructor named `{}`", node->matchType->str(), constructorName);
    node->constructorTag = result.first;
    node->valueConstructor = result.second;

    Symbol* symbol = resolveSymbol(constructorName);
    CHECK(symbol, "constructor `{}` is not defined", constructorName);

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

    CHECK(constructedType->valueConstructors().size() == 1, "let statement pattern matching only applies to types with a single constructor");
    CHECK(functionType->inputs().size() == node->params.size(), "constructor pattern `{}` does not have the correct number of arguments", constructor);

    node->valueConstructor = constructedType->valueConstructors()[0];

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
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(AssignNode* node)
{
    node->lhs->accept(this);

    Symbol* symbol;
    if (NullaryNode* lhs = dynamic_cast<NullaryNode*>(node->lhs))
    {
        symbol = lhs->symbol;
    }
    else if (MemberAccessNode* lhs = dynamic_cast<MemberAccessNode*>(node->lhs))
    {
        symbol = lhs->symbol;
    }
    else
    {
        semanticError(node->location, "left-hand side of assignment statement is not an lvalue");
        return;
    }

    CHECK(symbol->kind == kVariable || symbol->kind == kMemberVar, "symbol `{}` is not a variable", symbol->name);
    node->symbol = symbol;

	node->rhs->accept(this);

    unify(node->lhs->type, node->rhs->type, node);

    node->type = _typeTable->Unit;
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
    unify(node->lhs->type, _typeTable->Num, node->lhs);

    node->rhs->accept(this);
    unify(node->rhs->type, _typeTable->Num, node->rhs);

    unify(node->lhs->type, node->rhs->type, node);

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
    if (srcType->equals(destType)) return;
    if (srcType->equals(_typeTable->Int) && destType->equals(_typeTable->UInt)) return;
    if (srcType->equals(_typeTable->Int) && destType->equals(_typeTable->UInt8)) return;
    if (srcType->equals(_typeTable->UInt) && destType->equals(_typeTable->Int)) return;
    if (srcType->equals(_typeTable->UInt) && destType->equals(_typeTable->UInt8)) return;
    if (srcType->equals(_typeTable->UInt8) && destType->equals(_typeTable->Int)) return;
    if (srcType->equals(_typeTable->UInt8) && destType->equals(_typeTable->UInt)) return;

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
        if (functionType->get<FunctionType>()->inputs().empty())
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
    unify(node->lhs->type, _typeTable->Num, node->lhs);

    node->rhs->accept(this);
    unify(node->rhs->type, _typeTable->Num, node->rhs);

    unify(node->lhs->type, node->rhs->type, node);

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

void SemanticAnalyzer::visit(IfNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, _typeTable->Bool, node);

    _symbolTable->pushScope();
    node->body->accept(this);
    _symbolTable->popScope();
    unify(node->body->type, _typeTable->Unit, node);

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(IfElseNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, _typeTable->Bool, node);

    _symbolTable->pushScope();
    node->body->accept(this);
    _symbolTable->popScope();

    _symbolTable->pushScope();
    node->elseBody->accept(this);
    _symbolTable->popScope();

    unify(node->body->type, node->elseBody->type, node);
    node->type = node->body->type;
}

void SemanticAnalyzer::visit(AssertNode* node)
{
    node->condition->accept(this);
    unify(node->condition->type, _typeTable->Bool, node);

    // HACK: Give the code generator access to these symbols
    node->panicSymbol = dynamic_cast<FunctionSymbol*>(resolveSymbol("panic"));

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

void SemanticAnalyzer::visit(ForeachNode* node)
{
    node->listExpression->accept(this);
    Type* iteratorType = node->listExpression->type;

    // Save the current inner-most loop so that we can restore it after
    // visiting the children of this loop.
    LoopNode* outerLoop = _enclosingLoop;

    node->type = _typeTable->Unit;

    _enclosingLoop = node;
    _symbolTable->pushScope();

    CHECK(node->varName != "_", "for-loop induction variable cannot be unnamed");
    CHECK_UNDEFINED_IN_SCOPE(node->varName);

    // HACK: Check that the rhs is an instance of Iterator
    TraitSymbol* iteratorSymbol = dynamic_cast<TraitSymbol*>(resolveTypeSymbol("Iterator"));
    assert(iteratorSymbol);
    Type* varType = _typeTable->createTypeVariable();
    Trait* Iterator = iteratorSymbol->trait->instantiate({varType});

    //std::cerr << "before: " << iteratorType->str() << " -- " << Iterator->str() << std::endl;
    unify(iteratorType, Iterator, node->listExpression);
    //std::cerr << "after: " << iteratorType->str() << " -- " << Iterator->str() << std::endl;

    // HACK: Give the code generator access to these symbols
    std::vector<MemberSymbol*> symbols;
    _symbolTable->resolveMemberSymbol("head", iteratorType, symbols);
    node->headSymbol = symbols.front();
    Type* headType = instantiate(node->headSymbol->type, node->headTypeAssignment);
    node->varType = headType->get<FunctionType>()->output();
    unify(headType, _typeTable->createFunctionType({iteratorType}, node->varType), node);

    _symbolTable->resolveMemberSymbol("tail", iteratorType, symbols);
    node->tailSymbol = symbols.front();
    Type* tailType = instantiate(node->tailSymbol->type, node->tailTypeAssignment);
    unify(tailType, _typeTable->createFunctionType({iteratorType}, iteratorType), node);

    _symbolTable->resolveMemberSymbol("empty", iteratorType, symbols);
    node->emptySymbol = symbols.front();
    Type* emptyType = instantiate(node->emptySymbol->type, node->emptyTypeAssignment);
    unify(emptyType, _typeTable->createFunctionType({iteratorType}, _typeTable->Bool), node);



    VariableSymbol* symbol = _symbolTable->createVariableSymbol(node->varName, node, _enclosingFunction, false);
    symbol->type = node->varType;
    node->symbol = symbol;

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
    node->type = _typeTable->String;

    std::string name = "__staticString" + std::to_string(node->counter);
    VariableSymbol* symbol = _symbolTable->createVariableSymbol(name, node, nullptr, true);
    symbol->isStatic = true;
    symbol->contents = node->content;
    node->symbol = symbol;
}

void SemanticAnalyzer::visit(ReturnNode* node)
{
    CHECK(_enclosingFunction, "Cannot return from top level");

    node->expression->accept(this);

    Type* type = _enclosingFunction->symbol->type;
    assert(type->tag() == ttFunction);

    // Value of expression must equal the return type of the enclosing function.
    FunctionType* functionType = type->get<FunctionType>();

    unify(node->expression->type, functionType->output(), node);
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
        std::unordered_map<std::string, Type*> typeContext;
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

        _typeContexts.push_back(typeContext);
        for (size_t i = 0; i < node->constructorSpecs.size(); ++i)
        {
            auto& spec = node->constructorSpecs[i];
            spec->constructorTag = i;
            spec->resultType = newType;
            spec->accept(this);

            node->valueConstructors.push_back(spec->valueConstructor);
            node->constructorSymbols.push_back(spec->symbol);
        }
        _typeContexts.pop_back();
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

    CHECK(!node->members.empty(), "structs cannot be empty");

    // TODO: Refactor these two cases (and maybe DataDeclaration as well)
    if (node->typeParameters.empty())
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
        // Create type variables for each type parameter
        std::vector<Type*> variables;
        std::unordered_map<std::string, Type*> typeContext;
        resolveTypeParams(node, node->typeParameters, typeContext, variables);

        Type* newType = _typeTable->createConstructedType(typeName, std::move(variables));
        _symbolTable->createTypeSymbol(typeName, node, newType);

        _typeContexts.push_back(typeContext);

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

        _typeContexts.pop_back();

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

    // Make sure the object's type is an instance of Index
    TraitSymbol* indexSymbol = dynamic_cast<TraitSymbol*>(resolveTypeSymbol("Index"));
    assert(indexSymbol);

    Type* inputType = node->index->type;
    Type* outputType = _typeTable->createTypeVariable();
    Trait* Index = indexSymbol->trait->instantiate({inputType, outputType});
    unify(node->object->type, Index, node->object);

    // Find the at method
    std::vector<MemberSymbol*> symbols;
    _symbolTable->resolveMemberSymbol("at", node->object->type, symbols);
    node->atSymbol = dynamic_cast<MethodSymbol*>(symbols.front());
    Type* atType = instantiate(node->atSymbol->type, node->typeAssignment);

    Type* expectedType = _typeTable->createFunctionType({node->object->type, node->index->type}, outputType);
    unify(atType, expectedType, node);

    node->type = outputType;
}

void SemanticAnalyzer::visit(ImplNode* node)
{
    CHECK_TOP_LEVEL("method implementation block");

    assert(!_enclosingImplNode);
    _enclosingImplNode = node;

    // Create type variables for each type parameter
    std::unordered_map<std::string, Type*> typeContext;
    resolveTypeParams(node, node->typeParams, typeContext);
    _typeContexts.push_back(typeContext);

    TraitSymbol* traitSymbol = nullptr;
    std::vector<Type*> traitParameters;
    if (node->traitName)
    {
        traitSymbol = resolveTrait(node->traitName, traitParameters);
    }

    resolveTypeName(node->typeName);

    // Don't allow extraneous type variables, like this:
    // impl<T> Test. This also implies that if this is a trait impl block, then
    // the trait is no more generic than the object type. In other words, for
    // each concrete object type, there is a unique concrete trait
    for (auto& item : typeContext)
    {
        CHECK(occurs(item.second->get<TypeVariable>(), node->typeName->type),
            "type variable `{}` doesn't occur in type `{}`",
            item.first,
            node->typeName->type->str());
    }

    node->typeContext = typeContext;

    // First pass: check prototype, and create symbol
    std::unordered_map<std::string, Type*> methods;
    for (auto& method : node->methods)
    {
        method->accept(this);
        methods[method->name] = method->typeName->type;
    }

    // If a trait method block, then check that we actually have implementations
    // for each trait method
    if (traitSymbol)
    {
        // Check for overlapping trait instances
        CHECK(!hasOverlappingInstance(traitSymbol->trait, node->typeName->type),
            "trait `{}` already has an instance which would overlap with `{}`",
            traitSymbol->name, node->typeName->type->str());

        TypeAssignment traitSub;
        traitSub[traitSymbol->traitVar->get<TypeVariable>()] = node->typeName->type;

        for (auto& item : traitSymbol->methods)
        {
            std::string name = item.first;
            Type* type = instantiate(item.second, traitSub);

            auto i = methods.find(name);
            CHECK(i != methods.end(), "no implementation was given for method `{}` in trait `{}`", name, traitSymbol->name);

            unify(type, i->second, node);
        }

        // Make sure there aren't any extra methods as well
        for (auto& item : methods)
        {
            CHECK(traitSymbol->methods.find(item.first) != traitSymbol->methods.end(), "method `{}` is not a member of trait `{}`", item.first, traitSymbol->name);
        }

        traitSymbol->trait->addInstance(node->typeName->type, traitParameters);
    }

    // Second pass: check the method body
    for (auto& method: node->methods)
    {
        method->accept(this);
    }

    _typeContexts.pop_back();
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

        // Create type variables for each type parameter
        std::unordered_map<std::string, Type*> typeContext = _enclosingImplNode->typeContext;
        resolveTypeParams(node, node->typeParams, typeContext);

        _typeContexts.push_back(typeContext);
        resolveTypeName(node->typeName);

        std::vector<MemberSymbol*> symbols;
        Type* parentType = _enclosingImplNode->typeName->type;
        _symbolTable->resolveMemberSymbol(node->name, parentType, symbols);
        CHECK(symbols.empty(), "type `{}` already has a method or member named `{}`", parentType->str(), node->name);

        Type* type = node->typeName->type;
        FunctionType* functionType = type->get<FunctionType>();
        node->functionType = functionType;

        assert(functionType->inputs().size() == node->params.size());

        const std::vector<Type*>& paramTypes = functionType->inputs();

        MethodSymbol* symbol = _symbolTable->createMethodSymbol(node->name, node, parentType);
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
        _typeContexts.pop_back();

        node->type = _typeTable->Unit;
    }
}

void SemanticAnalyzer::visit(TraitDefNode* node)
{
    CHECK_TOP_LEVEL("trait definition");

    // The type name cannot have already been used for something
    const std::string& traitName = node->name;
    CHECK_UNDEFINED(traitName);

    // Create type variables for each type parameter
    std::unordered_map<std::string, Type*> typeContext;
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

    // Recurse to methods
    _enclosingTraitDef = node;
    _typeContexts.push_back(typeContext);
    for (auto& method : node->methods)
    {
        method->accept(this);
    }
    _typeContexts.pop_back();
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

    std::unordered_map<std::string, Type*> typeContext;
    typeContext["Self"] = traitSymbol->traitVar;

    _typeContexts.push_back(typeContext);
    resolveTypeName(node->typeName);
    _typeContexts.pop_back();

    Type* type = node->typeName->type;
    FunctionType* functionType = type->get<FunctionType>();

    const std::vector<Type*>& paramTypes = functionType->inputs();

    methods[name] = type;

    TraitMethodSymbol* symbol = _symbolTable->createTraitMethodSymbol(node->name, node, traitSymbol);
    symbol->type = type;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(PassNode* node)
{
    node->type = _typeTable->Unit;
}


//// SemanticPass2 /////////////////////////////////////////////////////////////

SemanticPass2::SemanticPass2(AstContext* context)
: _context(context)
, _typeTable(context->typeTable())
{
}

bool SemanticPass2::analyze()
{
    try
    {
        _context->root()->accept(this);
    }
    catch (std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }

    return true;
}

void SemanticPass2::visit(BinopNode* node)
{
    Type* type = node->type;

    if (type->isVariable() && !type->get<TypeVariable>()->quantified())
    {
        semanticError(node->location, "Cannot infer the type of arguments to binary operator");
    }
}

void SemanticPass2::visit(ComparisonNode* node)
{
    Type* type = node->lhs->type;

    // Quantified variables will be replaced with concrete types during monomorphisation
    if (type->isVariable() && !type->get<TypeVariable>()->quantified())
    {
        semanticError(node->location, "Cannot infer the type of arguments to comparison operator");
    }
}

void SemanticPass2::visit(IntNode* node)
{
    Type* type = node->type;

    if (type->isVariable() && !type->get<TypeVariable>()->quantified())
    {
        // If no specific type was inferred, then assume Int
        unify(type, _typeTable->Int, node);
    }
}
