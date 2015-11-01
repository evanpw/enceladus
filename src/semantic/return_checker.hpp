#ifndef RETURN_CHECKER_HPP
#define RETURN_CHECKER_HPP

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"

#define NORETURN(T) virtual void visit(T* node) { _alwaysReturns = false; }

class ReturnChecker : public AstVisitor
{
public:
    bool checkFunction(FunctionDefNode* function);
    bool checkMethod(MethodDefNode* method);

    // Relevant nodes
    virtual void visit(BlockNode* node);
    virtual void visit(ForeachNode* node);
    virtual void visit(ForeverNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(IfNode* node);
    virtual void visit(MatchArm* node);
    virtual void visit(MatchNode* node);
    virtual void visit(ReturnNode* node);
    virtual void visit(WhileNode* node);

    // Nodes that can never contain a return
    NORETURN(BoolNode);
    NORETURN(IntNode);
    NORETURN(NullaryNode);
    NORETURN(PassNode);
    NORETURN(StringLiteralNode);
    NORETURN(AssertNode);
    NORETURN(AssignNode);
    NORETURN(LetNode);
    NORETURN(VariableDefNode);
    NORETURN(MemberAccessNode);
    NORETURN(MethodCallNode);
    NORETURN(IndexNode);
    NORETURN(LogicalNode);
    NORETURN(CastNode);
    NORETURN(ComparisonNode);
    NORETURN(BinopNode);
    NORETURN(BreakNode);

    // Top-level nodes: should never occur within a function
    UNSUPPORTED(ConstructorSpec);
    UNSUPPORTED(DataDeclaration);
    UNSUPPORTED(ForeignDeclNode);
    UNSUPPORTED(FunctionDefNode);
    UNSUPPORTED(ImplNode);
    UNSUPPORTED(MemberDefNode);
    UNSUPPORTED(MethodDefNode);
    UNSUPPORTED(ProgramNode);
    UNSUPPORTED(StructDefNode);
    UNSUPPORTED(TraitDefNode);
    UNSUPPORTED(TraitMethodNode);
    UNSUPPORTED(TypeAliasNode);

private:
    bool _alwaysReturns;

    bool visitAndGet(AstNode* node)
    {
        node->accept(this);
        return _alwaysReturns;
    }
};

#endif
