#ifndef SYMBOL_HPP
#define SYMBOL_HPP

class AstNode;
class FunctionDefNode;

enum Kind {kVariable = 0, kFunction = 1};

struct Symbol
{
    Symbol(const char* name, Kind kind, AstNode* node, FunctionDefNode* enclosingFunction, bool isParam = false)
    : name(name), kind(kind), node(node), enclosingFunction(enclosingFunction), isParam(isParam), offset(0) {}

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
};

#endif
