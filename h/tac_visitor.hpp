#ifndef TAC_VISITOR_HPP
#define TAC_VISITOR_HPP

struct TACAssign;
struct TACBinaryOperation;
struct TACCall;
struct TACComment;
struct TACConditionalJump;
struct TACIndirectCall;
struct TACJump;
struct TACJumpIf;
struct TACJumpIfNot;
struct TACLeftIndexedAssignment;
struct TACProgram;
struct TACReturn;
struct TACRightIndexedAssignment;

struct TACVisitor
{
    virtual ~TACVisitor() {}

    virtual void visit(TACAssign* inst) = 0;
    virtual void visit(TACBinaryOperation* inst) = 0;
    virtual void visit(TACCall* inst) = 0;
    virtual void visit(TACComment* inst) {}
    virtual void visit(TACConditionalJump* inst) = 0;
    virtual void visit(TACIndirectCall* inst) = 0;
    virtual void visit(TACJump* inst) = 0;
    virtual void visit(TACJumpIf* inst) = 0;
    virtual void visit(TACJumpIfNot* inst) = 0;
    virtual void visit(TACLeftIndexedAssignment* inst) = 0;
    virtual void visit(TACReturn* inst) {}
    virtual void visit(TACRightIndexedAssignment* inst) = 0;
};

#endif
