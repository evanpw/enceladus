#include "semantic/return_checker.hpp"

bool ReturnChecker::checkFunction(FunctionDefNode* function)
{
    return visitAndGet(function->body);
}

bool ReturnChecker::checkMethod(MethodDefNode* method)
{
    return visitAndGet(method->body);
}

void ReturnChecker::visit(BlockNode* node)
{
    for (auto& child : node->children)
    {
        if (visitAndGet(child))
        {
            _alwaysReturns = true;
            return;
        }
    }

    _alwaysReturns = false;
}

void ReturnChecker::visit(ForNode* node)
{
    _alwaysReturns = visitAndGet(node->body);
}

void ReturnChecker::visit(FunctionCallNode* node)
{
    // HACK
    _alwaysReturns = (node->target == "panic");
}

void ReturnChecker::visit(IfElseNode* node)
{
    _alwaysReturns = visitAndGet(node->body) && node->elseBody && visitAndGet(node->elseBody);
}

void ReturnChecker::visit(ForeverNode* node)
{
    LoopEscapeChecker loopChecker(node);
    _alwaysReturns = !loopChecker.canEscape() || visitAndGet(node->body);
}

void ReturnChecker::visit(MatchArm* node)
{
    _alwaysReturns = visitAndGet(node->body);
}

void ReturnChecker::visit(MatchNode* node)
{
    for (auto& arm : node->arms)
    {
        if (!visitAndGet(arm))
        {
            _alwaysReturns = false;
            return;
        }
    }

    _alwaysReturns = true;
}

void ReturnChecker::visit(ReturnNode* node)
{
    _alwaysReturns = true;
}

void ReturnChecker::visit(WhileNode* node)
{
    _alwaysReturns = visitAndGet(node->body);
}
