#include "symbol.hpp"

Symbol* makeVariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction)
{
    Symbol* symbol = new Symbol(name, kVariable, node, enclosingFunction);

    symbol->asVariable.isParam = false;
    symbol->asVariable.offset = 0;

    return symbol;
}

Symbol* makeFunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition)
{
    Symbol* symbol = new Symbol(name, kFunction, node, nullptr);

    symbol->asFunction.isForeign = false;
    symbol->asFunction.isExternal = false;
    symbol->asFunction.isBuiltin = false;
    symbol->asFunction.definition = definition;

    return symbol;
}
