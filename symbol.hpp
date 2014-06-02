#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include <cassert>
#include <vector>
#include "types.hpp"

class AstNode;
class FunctionDefNode;
struct Symbol;


struct VariableData
{
    // Is this symbol a function parameter?
    bool isParam;

    // Used by the code generator to assign a place on the stack (relative to rbp) for all of
    // the local variables.
    int offset;
};

Symbol* makeVariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction);


struct FunctionData
{
    // C argument-passing style
    bool isForeign;

    bool isExternal;

    bool isBuiltin;

    FunctionDefNode* definition;
};

Symbol* makeFunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition);


enum Kind {kVariable = 0, kFunction = 1};

struct Symbol
{
    Symbol(const std::string& name, Kind kind, AstNode* node, FunctionDefNode* enclosingFunction)
    : name(name)
    , node(node)
    , enclosingFunction(enclosingFunction)
    , kind(kind)
    {}

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

    union
    {
        FunctionData asFunction;
        VariableData asVariable;
    };
};

#endif
