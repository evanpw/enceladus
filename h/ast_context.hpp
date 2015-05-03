#ifndef AST_CONTEXT_HPP
#define AST_CONTEXT_HPP

class AstNode;
class ProgramNode;

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

private:
    ProgramNode* _root = nullptr;
    std::vector<std::unique_ptr<AstNode>> _nodes;
};

#endif
