#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include "types.hpp"

class AstNode;
class FunctionDefNode;

enum Kind {kVariable = 0, kFunction = 1};

struct Symbol
{
    Symbol(const char* name, Kind kind, AstNode* node, FunctionDefNode* enclosingFunction)
    : name(name), kind(kind), node(node), enclosingFunction(enclosingFunction), isParam(false), offset(0), type(&Type::Void) {}

    // This name MUST BE stored in the string table. The symbol table is indexed by pointer,
    // not by string, so there cannot be multiple copies of the same string or bad things will
    // happen.
    const char* name;

    // Label, variable, function, ...?
    Kind kind;

    // The node at which this symbol is first declared.
    AstNode* node;

    // If a global symbol, then null
    FunctionDefNode* enclosingFunction;

    // Is this symbol a function parameter?
    bool isParam;

    // Used by the code generator to assign a place on the stack (relative to rbp) for all of
    // the local variables.
    int offset;

    // For variables, the type; for functions, the type of the return value
    const Type* type;

    // Valid only for functions
    unsigned int arity;
};

#endif
