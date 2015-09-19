#ifndef AST_CONTEXT_HPP
#define AST_CONTEXT_HPP

class AstNode;
class ProgramNode;

#include "semantic/symbol_table.hpp"
#include "semantic/types.hpp"

#include <memory>
#include <utility>
#include <vector>

class AstContext
{
public:
    void setRoot(ProgramNode* node) { _root = node; }
    ProgramNode* root() { return _root; }

    // The context takes ownership of this pointer
    void addToContext(AstNode* node);

    TypeTable* typeTable() { return &_typeTable; }
    SymbolTable* symbolTable() { return &_symbolTable; }

private:
    ProgramNode* _root = nullptr;
    std::vector<std::unique_ptr<AstNode>> _nodes;
    TypeTable _typeTable;
    SymbolTable _symbolTable;
};

#endif
