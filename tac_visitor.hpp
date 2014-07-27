#ifndef TAC_VISITOR_HPP
#define TAC_VISITOR_HPP

struct TACConditionalJump;
struct TACJumpIf;
struct TACJumpIfNot;
struct TACAssign;
struct TACJump;
struct TACLabel;
struct TACCall;
struct TACIndirectCall;
struct TACRightIndexedAssignment;
struct TACLeftIndexedAssignment;
struct TACBinaryOperation;
struct TACProgram;

struct TACVisitor
{
    virtual ~TACVisitor() {}

    virtual void visit(TACConditionalJump* inst) = 0;
    virtual void visit(TACJumpIf* inst) = 0;
    virtual void visit(TACJumpIfNot* inst) = 0;
    virtual void visit(TACAssign* inst) = 0;
    virtual void visit(TACJump* inst) = 0;
    virtual void visit(TACLabel* inst) = 0;
    virtual void visit(TACCall* inst) = 0;
    virtual void visit(TACIndirectCall* inst) = 0;
    virtual void visit(TACRightIndexedAssignment* inst) = 0;
    virtual void visit(TACLeftIndexedAssignment* inst) = 0;
    virtual void visit(TACBinaryOperation* inst) = 0;
};

#endif
