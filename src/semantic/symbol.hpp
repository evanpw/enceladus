#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include "semantic/types.hpp"

#include <cassert>
#include <vector>

class AstNode;
class FunctionDefNode;

struct VariableSymbol;
struct FunctionSymbol;
struct TypeSymbol;
struct TypeConstructorSymbol;
struct MemberSymbol;


enum Kind {kVariable = 0, kFunction = 1, kType = 2, kTypeConstructor = 3, kMember = 4};

struct Symbol
{
    Symbol(const std::string& name, Kind kind, AstNode* node, FunctionDefNode* enclosingFunction, bool global)
    : name(name)
    , node(node)
    , enclosingFunction(enclosingFunction)
    , global(global)
    , kind(kind)
    {}

    // This is just so that we can use dynamic_cast
    virtual ~Symbol() {}

    std::string name;

    // The node at which this symbol is first declared.
    AstNode* node;

    // May be null
    FunctionDefNode* enclosingFunction;

    bool global;

    // Type (possibly polymorphic) of this variable or function
    Type* type = nullptr;

    // Variable, function, ...?
    Kind kind;

    VariableSymbol* asVariable();
    FunctionSymbol* asFunction();
    TypeSymbol* asType();
    TypeConstructorSymbol* asTypeConstructor();
    MemberSymbol* asMember();

    const VariableSymbol* asVariable() const;
    const FunctionSymbol* asFunction() const;
    const TypeSymbol* asType() const;
    const TypeConstructorSymbol* asTypeConstructor() const;
    const MemberSymbol* asMember() const;
};


struct VariableSymbol : public Symbol
{
    VariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction, bool global);

    // Is this symbol a function parameter?
    bool isParam = false;

    bool isStatic = false;

    // Used by the code generator to assign a place on the stack (relative to rbp) for all of
    // the local variables.
    int offset = -1;

    // For static strings
    std::string contents;
};

struct FunctionSymbol : public Symbol
{
    FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition);

    bool isForeign = false;     // C argument-passing style
    bool isExternal = false;
    bool isBuiltin = false;

    FunctionDefNode* definition;
};

struct TypeSymbol : public Symbol
{
    TypeSymbol(const std::string& name, AstNode* node, Type* type);
};

struct TypeConstructorSymbol : public Symbol
{
    // Takes ownership of the pointer
    TypeConstructorSymbol(const std::string& name, AstNode* node, TypeConstructor* typeConstructor);

    TypeConstructor* typeConstructor;
};

struct MemberSymbol : public Symbol
{
    MemberSymbol(const std::string& name, AstNode* node);

    size_t location;
};


#endif
