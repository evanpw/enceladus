#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include <cassert>
#include <vector>
#include "types.hpp"

class AstNode;
class FunctionDefNode;

struct VariableSymbol;
struct FunctionSymbol;
struct TypeSymbol;
struct TypeConstructorSymbol;


enum Kind {kVariable = 0, kFunction = 1, kType = 2, kTypeConstructor = 3};

struct Symbol
{
    Symbol(const std::string& name, Kind kind, AstNode* node, FunctionDefNode* enclosingFunction)
    : name(name)
    , node(node)
    , enclosingFunction(enclosingFunction)
    , kind(kind)
    {}

    // This is just so that we can use dynamic_cast
    virtual ~Symbol() {}

    std::string name;

    // The node at which this symbol is first declared.
    AstNode* node;

    // If a global symbol, then null
    FunctionDefNode* enclosingFunction;

    // Type (possibly polymorphic) of this variable or function
    std::shared_ptr<TypeScheme> typeScheme;
    std::shared_ptr<Type> type;

    void setType(const std::shared_ptr<Type>& newType)
    {
        type = newType;
        typeScheme = TypeScheme::trivial(newType);
    }

    void setTypeScheme(const std::shared_ptr<TypeScheme>& newTypeScheme)
    {
        type.reset();
        typeScheme = newTypeScheme;
    }

    // Variable, function, ...?
    Kind kind;

    VariableSymbol* asVariable();
    FunctionSymbol* asFunction();
    TypeSymbol* asType();
    TypeConstructorSymbol* asTypeConstructor();

    const VariableSymbol* asVariable() const;
    const FunctionSymbol* asFunction() const;
    const TypeSymbol* asType() const;
    const TypeConstructorSymbol* asTypeConstructor() const;
};


struct VariableSymbol : public Symbol
{
    VariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction);

    // Is this symbol a function parameter?
    bool isParam;

    // Used by the code generator to assign a place on the stack (relative to rbp) for all of
    // the local variables.
    int offset;
};

struct FunctionSymbol : public Symbol
{
    FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition);

    // C argument-passing style
    bool isForeign;

    bool isExternal;

    bool isBuiltin;

    FunctionDefNode* definition;
};

struct TypeSymbol : public Symbol
{
    TypeSymbol(const std::string& name, AstNode* node, std::shared_ptr<Type> type);
};

struct TypeConstructorSymbol : public Symbol
{
    // Takes ownership of the pointer
    TypeConstructorSymbol(const std::string& name, AstNode* node, TypeConstructor* typeConstructor);

    std::unique_ptr<TypeConstructor> typeConstructor;
};


#endif
