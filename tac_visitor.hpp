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

    virtual void visit(const TACConditionalJump* inst) = 0;
    virtual void visit(const TACJumpIf* inst) = 0;
    virtual void visit(const TACJumpIfNot* inst) = 0;
    virtual void visit(const TACAssign* inst) = 0;
    virtual void visit(const TACJump* inst) = 0;
    virtual void visit(const TACLabel* inst) = 0;
    virtual void visit(const TACCall* inst) = 0;
    virtual void visit(const TACIndirectCall* inst) = 0;
    virtual void visit(const TACRightIndexedAssignment* inst) = 0;
    virtual void visit(const TACLeftIndexedAssignment* inst) = 0;
    virtual void visit(const TACBinaryOperation* inst) = 0;
};

#endif
