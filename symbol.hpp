#ifndef SYMBOL_HPP
#define SYMBOL_HPP

class AstNode;

enum Kind {kVariable = 0, kFunction = 1};

struct Symbol
{
    Symbol(const char* name, Kind kind, AstNode* node, bool isParam = false)
    : name(name), kind(kind), node(node), isParam(isParam) {}

    // This name MUST BE stored in the string table. The symbol table is indexed by pointer,
    // not by string, so there cannot be multiple copies of the same string or bad things will
    // happen.
    const char* name;

    // Label, variable, function, ...?
    Kind kind;

    // The node at which this symbol is first declared.
    AstNode* node;

    // Is this symbol a function parameter?
    bool isParam;
};

#endif
