#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include <vector>
#include "types.hpp"

class AstNode;
class FunctionDefNode;

enum Kind {kVariable = 0, kFunction = 1};

struct Symbol
{
    Symbol(const std::string& name, Kind kind, AstNode* node, FunctionDefNode* enclosingFunction)
    : name(name)
    , kind(kind)
    , node(node)
    , enclosingFunction(enclosingFunction)
    {}

    std::string name;

    // Variable, function, ...?
    Kind kind;

    // The node at which this symbol is first declared.
    AstNode* node;

    // If a global symbol, then null
    FunctionDefNode* enclosingFunction;
};

struct VariableSymbol : public Symbol
{
    VariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction)
    : Symbol(name, kVariable, node, enclosingFunction)
    , isParam(false)
    , offset(0)
    , type(&Type::Void)
    {}

    // Is this symbol a function parameter?
    bool isParam;

    // Used by the code generator to assign a place on the stack (relative to rbp) for all of
    // the local variables.
    int offset;

    const Type* type;
};

struct FunctionSymbol : public Symbol
{
    FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction)
    : Symbol(name, kFunction, node, enclosingFunction)
    , type(&Type::Void)
    , arity(0)
    , isForeign(false)
    {}

    // Return type
    const Type* type;

    std::vector<const Type*> paramTypes;

    unsigned int arity;

    // Externally-linked C argument-passing style
    bool isForeign;
};

#endif
