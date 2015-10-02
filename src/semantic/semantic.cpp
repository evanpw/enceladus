#include "semantic/semantic.hpp"

#include "ast/ast_context.hpp"
#include "lib/library.h"
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
    CHECK(!_symbolTable->findTopScope(name), "symbol \"{}\" is already defined in this scope", name); \
    CHECK(!_symbolTable->findTopScope(name, SymbolTable::TYPE), "symbol \"{}\" is already defined in this scope", name)

#define CHECK_TOP_LEVEL(name) \
    CHECK(!_enclosingFunction, "{} must be at top level", name)

template<typename... Args>
void SemanticAnalyzer::semanticError(const YYLTYPE& location, const std::string& str, Args... args)
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

struct NotCompatible
{
    NotCompatible(Type* self)
    : self(self)
    {}

    bool operator()(MemberSymbol* symbol)
    {
        return !isCompatible(self, symbol->parentType);
    }

    Type* self;
};

void SemanticAnalyzer::resolveMemberSymbol(const std::string& name, Type* parentType, std::vector<MemberSymbol*>& symbols)
{
    _symbolTable->findMembers(name, symbols);

    // Filter out types that can't be unifed with parentType
    symbols.erase(std::remove_if(symbols.begin(), symbols.end(), NotCompatible(parentType)), symbols.end());
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
    //// Built-in types ////////////////////////////////////////////////////////
    _symbolTable->createTypeSymbol("Int", _root, _typeTable->Int);
    _symbolTable->createTypeSymbol("Bool", _root, _typeTable->Bool);
    _symbolTable->createTypeSymbol("Unit", _root, _typeTable->Unit);
    _symbolTable->createTypeSymbol("String", _root, _typeTable->String);

    _symbolTable->createTypeConstructorSymbol("Function", _root, _typeTable->Function);
    _symbolTable->createTypeConstructorSymbol("Array", _root, _typeTable->Array);


	//// Create symbols for built-in functions
    FunctionSymbol* notFn = createBuiltin("not");
	notFn->type = _typeTable->createFunctionType({_typeTable->Bool}, _typeTable->Bool);

	//// Integer arithmetic functions //////////////////////////////////////////

    Type* arithmeticType = _typeTable->createFunctionType({_typeTable->Int, _typeTable->Int}, _typeTable->Int);

	FunctionSymbol* add = createBuiltin("+");
	add->type = arithmeticType;

	FunctionSymbol* subtract = createBuiltin("-");
	subtract->type = arithmeticType;

	FunctionSymbol* multiply = createBuiltin("*");
	multiply->type = arithmeticType;

	FunctionSymbol* divide = createBuiltin("/");
	divide->type = arithmeticType;

	FunctionSymbol* modulus = createBuiltin("%");
	modulus->type = arithmeticType;

	//// These definitions are only needed so that we list them as external
	//// symbols in the output assembly file. They can't be called from
	//// language.
    FunctionSymbol* gcAllocate = _symbolTable->createFunctionSymbol("gcAllocate", _root, nullptr);
    gcAllocate->isExternal = true;

    _symbolTable->createFunctionSymbol("_main", _root, nullptr);
}

void SemanticAnalyzer::resolveBaseType(TypeName* typeName, const std::unordered_map<std::string, Type*>& variables)
{
    const std::string& name = typeName->name;

    auto i = variables.find(name);
    if (i != variables.end())
    {
        typeName->type = i->second;
        return;
    }

    Symbol* symbol = resolveTypeSymbol(name);
    CHECK_AT(typeName->location, symbol, "Base type \"{}\" is not defined", name);
    CHECK_AT(typeName->location, symbol->kind == kType, "Symbol \"{}\" is not a base type", name);

    typeName->type = symbol->type;
}

TypeConstructor* SemanticAnalyzer::getTypeConstructor(const TypeName* typeName)
{
    return getTypeConstructor(typeName->location, typeName->name);
}

TypeConstructor* SemanticAnalyzer::getTypeConstructor(const YYLTYPE& location, const std::string& name)
{
    Symbol* symbol = resolveTypeSymbol(name);
    CHECK_AT(location, symbol, "Type constructor \"{}\" is not defined", name);
    CHECK_AT(location, symbol->kind == kTypeConstructor, "Symbol \"{}\" is not a type constructor", name);

    return dynamic_cast<TypeConstructorSymbol*>(symbol)->typeConstructor;
}

void SemanticAnalyzer::resolveTypeName(TypeName* typeName, const std::unordered_map<std::string, Type*>& variables)
{
    if (typeName->parameters.empty())
    {
        resolveBaseType(typeName, variables);
    }
    else
    {
        std::vector<Type*> typeParameters;
        for (auto& parameter : typeName->parameters)
        {
            resolveTypeName(parameter, variables);
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

//// Type inference functions //////////////////////////////////////////////////

void SemanticAnalyzer::bindVariable(Type* variable, Type* value, AstNode* node)
{
    assert(variable->tag() == ttVariable);

    Type* rhs = value;

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

    variable->assign(rhs);
}

void SemanticAnalyzer::inferenceError(AstNode* node, const std::string& msg)
{
    std::stringstream ss;

    auto location = node->location;

    ss << location.filename << ":" << location.first_line <<  ":" << location.first_column
       << ": " << msg;

    throw TypeInferenceError(ss.str());
}

Type* SemanticAnalyzer::instantiate(Type* type)
{
    std::map<TypeVariable*, Type*> replacements;
    return instantiate(type, replacements);
}

Type* SemanticAnalyzer::instantiate(Type* type, std::map<TypeVariable*, Type*>& replacements)
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
                if (typeVariable->quantified())
                {
                    Type* replacement = _typeTable->createTypeVariable();
                    replacements[typeVariable] = replacement;

                    return replacement;
                }
                else
                {
                    return type;
                }
            }
        }

        case ttFunction:
        {
            FunctionType* functionType = type->get<FunctionType>();

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

            ConstructedType* constructedType = type->get<ConstructedType>();
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

bool SemanticAnalyzer::occurs(TypeVariable* variable, Type* value)
{
    Type* rhs = value;

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

void SemanticAnalyzer::unify(Type* lhs, Type* rhs, AstNode* node)
{
    assert(lhs && rhs && node);

    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        // Two base types can be unified only if equal (we don't have inheritance)
        if (lhs->equals(rhs))
            return;
    }
    else if (lhs->tag() == ttVariable)
    {
        // Non-quantified type variables can always be bound
        if (!lhs->get<TypeVariable>()->quantified())
        {
            bindVariable(lhs, rhs, node);
            return;
        }
        else
        {
            // Trying to unify a quantified type variable with a type that is not a
            // variable is always an error
            if (rhs->tag() == ttVariable)
            {
                // A quantified type variable unifies with itself
                if (lhs->equals(rhs))
                {
                    return;
                }

                // And non-quantified type variables can be bound to quantified ones
                else if (!rhs->get<TypeVariable>()->quantified())
                {
                    bindVariable(rhs, lhs, node);
                    return;
                }
             }
        }
    }
    else if (rhs->tag() == ttVariable && !rhs->get<TypeVariable>()->quantified())
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
    for (auto& typeParameter : node->typeParams)
    {
        CHECK_UNDEFINED(typeParameter);
        CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter \"{}\" is already defined", typeParameter);

        Type* var = _typeTable->createTypeVariable(typeParameter, true);
        typeContext.emplace(typeParameter, var);
    }

    resolveTypeName(node->typeName, typeContext);
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

    // Create type variables for each type parameter
    std::unordered_map<std::string, Type*> typeContext;
    for (auto& typeParameter : node->typeParams)
    {
        CHECK_UNDEFINED(typeParameter);
        CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter \"{}\" is already defined", typeParameter);

        Type* var = _typeTable->createTypeVariable(typeParameter, true);
        typeContext.emplace(typeParameter, var);
    }

    resolveTypeName(node->typeName, typeContext);

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
	// Visit children. Do this first so that we can't have recursive definitions.
	AstVisitor::visit(node);

	const std::string& target = node->target;
    if (target != "_")
    {
        CHECK_UNDEFINED_IN_SCOPE(target);

        bool global = _symbolTable->isTopScope();
    	VariableSymbol* symbol = _symbolTable->createVariableSymbol(target, node, _enclosingFunction, global);

    	if (node->typeName)
    	{
    		resolveTypeName(node->typeName);
    		symbol->type = node->typeName->type;
    	}
    	else
    	{
    		symbol->type = _typeTable->createTypeVariable();
    	}

    	node->symbol = symbol;

    	unify(node->rhs->type, symbol->type, node);
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

    Symbol* symbol = resolveSymbol(constructorName);
    CHECK(symbol, "constructor \"{}\" is not defined", constructorName);

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
        "constructor pattern \"{}\" does not have the correct number of arguments", constructorName);

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
    AstVisitor::visit(node);

    _symbolTable->popScope();

    node->type = node->body->type;
}

void SemanticAnalyzer::visit(LetNode* node)
{
	AstVisitor::visit(node);

	const std::string& constructor = node->constructor;
	Symbol* symbol = resolveSymbol(constructor);
    CHECK(symbol, "constructor \"{}\" is not defined", constructor);

    ConstructorSymbol* constructorSymbol = dynamic_cast<ConstructorSymbol*>(symbol);
    CHECK(constructorSymbol, "\"{}\" is not a value constructor", constructor);
    node->constructorSymbol = constructorSymbol;

	// A symbol with a capital letter should always be a constructor
	assert(constructorSymbol->kind == kFunction);

	assert(constructorSymbol->type->tag() == ttFunction);
    Type* instantiatedType = instantiate(constructorSymbol->type, node->typeAssignment);
	FunctionType* functionType = instantiatedType->get<FunctionType>();
	const Type* constructedType = functionType->output();

    CHECK(constructedType->valueConstructors().size() == 1, "let statement pattern matching only applies to types with a single constructor");
    CHECK(functionType->inputs().size() == node->params.size(), "constructor pattern \"{}\" does not have the correct number of arguments", constructor);

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
    if (VariableNode* lhs = dynamic_cast<VariableNode*>(node->lhs))
    {
        symbol = lhs->symbol;
    }
    else if (NullaryNode* lhs = dynamic_cast<NullaryNode*>(node->lhs))
    {
        symbol = lhs->symbol;
    }
    else if (MemberAccessNode* lhs = dynamic_cast<MemberAccessNode*>(node->lhs))
    {
        symbol = lhs->symbol;
    }
    else
    {
        CHECK(false, "left-hand side of assignment statement is not an lvalue");
    }

    CHECK(symbol->kind == kVariable || symbol->kind == kMemberVar, "symbol \"{}\" is not a variable", symbol->name);
    node->symbol = symbol;

	node->rhs->accept(this);

    unify(node->lhs->type, node->rhs->type, node);

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
        AstNode* argument = node->arguments[i];
        argument->accept(this);

        paramTypes.push_back(argument->type);
    }

	node->symbol = symbol;

    Type* returnType = _typeTable->createTypeVariable();
    Type* functionType = instantiate(symbol->type, node->typeAssignment);

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
        Type* functionType = instantiate(symbol->type, node->typeAssignment);

        FunctionSymbol* functionSymbol = dynamic_cast<FunctionSymbol*>(symbol);
        if (functionType->get<FunctionType>()->inputs().empty())
        {
            Type* returnType = _typeTable->createTypeVariable();
            unify(functionType, _typeTable->createFunctionType({}, returnType), node);

            node->type = returnType;
            node->kind = NullaryNode::FUNC_CALL;
        }
        else
        {
            CHECK(!functionSymbol->isExternal, "Cannot put external function \"{}\" into a closure", name);

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
    node->dieSymbol = dynamic_cast<FunctionSymbol*>(resolveSymbol("die"));

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
    Type* varType = _typeTable->createTypeVariable();
    node->varType = varType;

    // Save the current inner-most loop so that we can restore it after
    // visiting the children of this loop.
    LoopNode* outerLoop = _enclosingLoop;

    node->type = _typeTable->Unit;

    _enclosingLoop = node;
    _symbolTable->pushScope();

    CHECK(node->varName != "_", "for-loop induction variable cannot be unnamed");
    CHECK_UNDEFINED_IN_SCOPE(node->varName);

    VariableSymbol* symbol = _symbolTable->createVariableSymbol(node->varName, node, _enclosingFunction, false);
    symbol->type = varType;
    node->symbol = symbol;

    // HACK: Give the code generator access to these symbols
    std::vector<MemberSymbol*> symbols;
    resolveMemberSymbol("head", iteratorType, symbols);
    node->headSymbol = dynamic_cast<MethodSymbol*>(symbols.front());
    Type* headType = instantiate(node->headSymbol->type, node->headTypeAssignment);
    unify(headType, _typeTable->createFunctionType({iteratorType}, varType), node);

    resolveMemberSymbol("tail", iteratorType, symbols);
    node->tailSymbol = dynamic_cast<MethodSymbol*>(symbols.front());
    Type* tailType = instantiate(node->tailSymbol->type, node->tailTypeAssignment);
    unify(tailType, _typeTable->createFunctionType({iteratorType}, iteratorType), node);

    resolveMemberSymbol("empty", iteratorType, symbols);
    node->emptySymbol = dynamic_cast<MethodSymbol*>(symbols.front());
    Type* emptyType = instantiate(node->emptySymbol->type, node->emptyTypeAssignment);
    unify(emptyType, _typeTable->createFunctionType({iteratorType}, _typeTable->Bool), node);

    node->body->accept(this);
    unify(node->body->type, _typeTable->Unit, node);

    _symbolTable->popScope();
    _enclosingLoop = outerLoop;

    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(ForNode* node)
{
    node->fromExpression->accept(this);
    unify(node->fromExpression->type, _typeTable->Int, node);

    node->toExpression->accept(this);
    unify(node->toExpression->type, _typeTable->Int, node);

    // Save the current inner-most loop so that we can restore it after
    // visiting the children of this loop.
    LoopNode* outerLoop = _enclosingLoop;

    node->type = _typeTable->Unit;

    _enclosingLoop = node;
    _symbolTable->pushScope();

    CHECK(node->varName != "_", "for-loop induction variable cannot be unnamed");
    CHECK_UNDEFINED_IN_SCOPE(node->varName);

    VariableSymbol* symbol = _symbolTable->createVariableSymbol(node->varName, node, _enclosingFunction, false);
    symbol->type = _typeTable->Int;
    node->symbol = symbol;

    node->type = _typeTable->Unit;
    node->body->accept(this);

    _symbolTable->popScope();
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
    _symbolTable->pushScope();

    node->body->accept(this);

    _symbolTable->popScope();
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
            CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter \"{}\" is already defined", typeParameter);

            Type* var = _typeTable->createTypeVariable(typeParameter, true);
            variables.push_back(var);

            typeContext.emplace(typeParameter, var);
        }

        TypeConstructor* typeConstructor = _typeTable->createTypeConstructor(name, node->typeParameters.size());
        TypeConstructorSymbol* symbol = _symbolTable->createTypeConstructorSymbol(name, node, typeConstructor);

        Type* newType = _typeTable->createConstructedType(typeConstructor, variables);

        for (size_t i = 0; i < node->constructorSpecs.size(); ++i)
        {
            auto& spec = node->constructorSpecs[i];
            spec->constructorTag = i;
            spec->typeContext = typeContext;
            spec->resultType = newType;
            spec->accept(this);

            typeConstructor->addValueConstructor(spec->valueConstructor);
            node->valueConstructors.push_back(spec->valueConstructor);
            node->constructorSymbols.push_back(spec->symbol);
        }
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

        resolveTypeName(member, node->typeContext);
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
            CHECK(alreadyUsed.find(member->name) == alreadyUsed.end(), "type \"{}\" already has a member named \"{}\"", node->name, member->name);
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
        for (auto& typeParameter : node->typeParameters)
        {
            CHECK_UNDEFINED(typeParameter);
            CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter \"{}\" is already defined", typeParameter);

            Type* var = _typeTable->createTypeVariable(typeParameter, true);
            variables.push_back(var);

            typeContext.emplace(typeParameter, var);
        }

        TypeConstructor* typeConstructor = _typeTable->createTypeConstructor(typeName, node->typeParameters.size());
        TypeConstructorSymbol* symbol = _symbolTable->createTypeConstructorSymbol(typeName, node, typeConstructor);

        Type* newType = _typeTable->createConstructedType(typeConstructor, variables);

        std::vector<std::string> memberNames;
        std::vector<Type*> memberTypes;
        std::vector<MemberVarSymbol*> memberSymbols;
        std::unordered_set<std::string> alreadyUsed;
        for (size_t i = 0; i < node->members.size(); ++i)
        {
            MemberDefNode* member = node->members[i];

            // Make sure there are no repeated member names
            CHECK(alreadyUsed.find(member->name) == alreadyUsed.end(), "type \"{}\" already has a member named \"{}\"", node->name, member->name);
            alreadyUsed.insert(member->name);

            member->typeContext = typeContext;
            member->accept(this);

            memberTypes.push_back(member->memberType);
            memberNames.push_back(member->name);

            MemberVarSymbol* memberSymbol = _symbolTable->createMemberVarSymbol(member->name, node, nullptr, newType, i);
            memberSymbol->type = _typeTable->createFunctionType({newType}, member->memberType);
            memberSymbols.push_back(memberSymbol);
        }

        ValueConstructor* valueConstructor = _typeTable->createValueConstructor(typeName, 0, memberTypes, memberNames);
        node->valueConstructor = valueConstructor;
        typeConstructor->addValueConstructor(valueConstructor);

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
    resolveTypeName(node->typeName, node->typeContext);
    node->memberType = node->typeName->type;
    node->type = _typeTable->Unit;
}

void SemanticAnalyzer::visit(MemberAccessNode* node)
{
    node->object->accept(this);
    Type* objectType = node->object->type;

    std::vector<MemberSymbol*> symbols;
    resolveMemberSymbol(node->memberName, objectType, symbols);
    CHECK(!symbols.empty(), "no member named \"{}\" found for type \"{}\"", node->memberName, objectType->name());
    assert(symbols.size() < 2);

    CHECK(symbols.front()->isMemberVar(), "\"{}\" is a method, not a member variable", node->memberName);
    MemberVarSymbol* symbol = dynamic_cast<MemberVarSymbol*>(symbols.front());
    node->symbol = symbol;

    Type* returnType = _typeTable->createTypeVariable();
    Type* functionType = instantiate(symbol->type, node->typeAssignment);

    unify(functionType, _typeTable->createFunctionType({objectType}, returnType), node);

    node->symbol = symbol;
    node->type = returnType;
    node->constructorSymbol = symbol->constructorSymbol;
    node->memberIndex = symbol->index;
}

void SemanticAnalyzer::visit(ImplNode* node)
{
    assert(!_enclosingImplNode);
    _enclosingImplNode = node;

    // Create type variables for each type parameter
    std::unordered_map<std::string, Type*> typeContext;
    for (auto& typeParameter : node->typeParams)
    {
        CHECK_UNDEFINED(typeParameter);
        CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter \"{}\" is already defined", typeParameter);

        Type* var = _typeTable->createTypeVariable(typeParameter, true);
        typeContext.emplace(typeParameter, var);
    }

    resolveTypeName(node->typeName, typeContext);

    node->typeContext = typeContext;

    // First pass: check prototype, and create symbol
    for (auto& method : node->methods)
    {
        method->accept(this);
    }

    // Second pass: check the method body
    for (auto& method: node->methods)
    {
        method->accept(this);
    }

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

        std::vector<MemberSymbol*> symbols;
        Type* parentType = _enclosingImplNode->typeName->type;
        resolveMemberSymbol(node->name, parentType, symbols);
        CHECK(symbols.empty(), "type \"{}\" already has a method or member named \"{}\"", parentType->name(), node->name);

        // Create type variables for each type parameter
        std::unordered_map<std::string, Type*> typeContext = _enclosingImplNode->typeContext;
        for (auto& typeParameter : node->typeParams)
        {
            CHECK_UNDEFINED(typeParameter);
            CHECK(typeContext.find(typeParameter) == typeContext.end(), "type parameter \"{}\" is already defined", typeParameter);

            Type* var = _typeTable->createTypeVariable(typeParameter, true);
            typeContext.emplace(typeParameter, var);
        }

        resolveTypeName(node->typeName, typeContext);

        Type* type = node->typeName->type;
        FunctionType* functionType = type->get<FunctionType>();
        node->functionType = functionType;

        assert(functionType->inputs().size() == node->params.size());

        const std::vector<Type*>& paramTypes = functionType->inputs();

        CHECK(!paramTypes.empty(), "methods must take at least one argument");
        unify(paramTypes[0], parentType, node);

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

        unify(node->body->type, functionType->output(), node);
        node->type = _typeTable->Unit;
    }
}

void SemanticAnalyzer::visit(MethodCallNode* node)
{
    node->object->accept(this);
    Type* objectType = node->object->type;

    std::vector<MemberSymbol*> symbols;
    resolveMemberSymbol(node->methodName, objectType, symbols);
    CHECK(!symbols.empty(), "no method named \"{}\" found for type \"{}\"", node->methodName, objectType->name());
    CHECK(symbols.size() < 2, "method call is amiguous");

    CHECK(symbols.front()->isMethod(), "\"{}\" is a member variable, not a method", node->methodName);
    MethodSymbol* symbol = dynamic_cast<MethodSymbol*>(symbols.front());

    std::vector<Type*> paramTypes = {objectType};
    for (size_t i = 0; i < node->arguments.size(); ++i)
    {
        AstNode& argument = *node->arguments[i];
        argument.accept(this);

        paramTypes.push_back(argument.type);
    }

    node->symbol = symbol;

    Type* returnType = _typeTable->createTypeVariable();
    Type* functionType = instantiate(symbol->type, node->typeAssignment);

    unify(functionType, _typeTable->createFunctionType(paramTypes, returnType), node);

    node->type = returnType;
}

void SemanticAnalyzer::visit(PassNode* node)
{
    node->type = _typeTable->Unit;
}