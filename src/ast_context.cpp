#include "ast_context.hpp"
#include "ast.hpp"

// The context takes ownership of this pointer
void AstContext::addToContext(AstNode* node)
{
    _nodes.emplace_back(node);
}
