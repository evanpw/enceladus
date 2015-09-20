#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include "semantic/types.hpp"

#include <cassert>
#include <vector>

class AstNode;
class FunctionDefNode;
class FunctionDeclNode;
class TraitDefNode;
class SymbolTable;


enum Kind {kVariable = 0, kFunction = 1, kType = 2, kTypeConstructor = 3, kMember = 4, kMethod = 5};

class Symbol
{
public:
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

protected:
    Symbol(const std::string& name, Kind kind, AstNode* node, FunctionDefNode* enclosingFunction, bool global)
    : name(name)
    , node(node)
    , enclosingFunction(enclosingFunction)
    , global(global)
    , kind(kind)
    {}
};


class VariableSymbol : public Symbol
{
public:
    // Is this symbol a function parameter?
    bool isParam = false;

    bool isStatic = false;

    // Used by the code generator to assign a place on the stack (relative to rbp) for all of
    // the local variables.
    int offset = -1;

    // For static strings
    std::string contents;

private:
    friend class SymbolTable;
    VariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction, bool global);
};

class FunctionSymbol : public Symbol
{
public:
    bool isForeign = false;     // C argument-passing style
    bool isExternal = false;
    bool isBuiltin = false;

    FunctionDefNode* definition;

private:
    friend class SymbolTable;
    FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition);
};

class TypeSymbol : public Symbol
{
private:
    friend class SymbolTable;
    TypeSymbol(const std::string& name, AstNode* node, Type* type);
};

class TypeConstructorSymbol : public Symbol
{
public:
    TypeConstructor* typeConstructor;

private:
    friend class SymbolTable;
    TypeConstructorSymbol(const std::string& name, AstNode* node, TypeConstructor* typeConstructor);
};

struct MemberSymbol : public Symbol
{
public:
    size_t location;

private:
    friend class SymbolTable;
    MemberSymbol(const std::string& name, AstNode* node);
};


class MethodSymbol : public Symbol
{
public:
    FunctionDefNode* definition;
    Type* parentType;

    // A number which is unique among methods with the same name
    size_t index;

private:
    friend class SymbolTable;
    MethodSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition, Type* parentType, size_t index);
};

#endif
