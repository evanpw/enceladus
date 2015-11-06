#ifndef RETURN_CHECKER_HPP
#define RETURN_CHECKER_HPP

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"

#define NORETURN(T) virtual void visit(T* node) { _alwaysReturns = false; }

class ReturnChecker : public SparseAstVisitor
{
public:
    bool checkFunction(FunctionDefNode* function);
    bool checkMethod(MethodDefNode* method);

    // Relevant nodes
    virtual void visit(BlockNode* node);
    virtual void visit(ForeverNode* node);
    virtual void visit(ForNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(MatchArm* node);
    virtual void visit(MatchNode* node);
    virtual void visit(ReturnNode* node);
    virtual void visit(WhileNode* node);

    // Nodes that can never contain a return
    NORETURN(AssertNode);
    NORETURN(AssignNode);
    NORETURN(BinopNode);
    NORETURN(BoolNode);
    NORETURN(BreakNode);
    NORETURN(CastNode);
    NORETURN(ComparisonNode);
    NORETURN(ContinueNode);
    NORETURN(IndexNode);
    NORETURN(IntNode);
    NORETURN(LetNode);
    NORETURN(LogicalNode);
    NORETURN(MemberAccessNode);
    NORETURN(MethodCallNode);
    NORETURN(NullaryNode);
    NORETURN(PassNode);
    NORETURN(StringLiteralNode);
    NORETURN(VariableDefNode);

private:
    bool _alwaysReturns;

    bool visitAndGet(AstNode* node)
    {
        node->accept(this);
        return _alwaysReturns;
    }
};

// Is there a break statement that allows a forever loop to be escaped?
class LoopEscapeChecker : public AstVisitor
{
public:
    LoopEscapeChecker(ForeverNode* loop)
    : _loop(loop)
    {}

    bool canEscape()
    {
        _loop->accept(this);
        return _loopEscapes;
    }

    virtual void visit(BreakNode* node)
    {
        if (node->loop == _loop)
        {
            _loopEscapes = true;
        }
    }

private:
    ForeverNode* _loop;
    bool _loopEscapes = false;
};

#endif
