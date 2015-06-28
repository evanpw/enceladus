#ifndef TAC_VISITOR_HPP
#define TAC_VISITOR_HPP

struct AssignInst;
struct BinaryOperationInst;
struct CallInst;
struct ConditionalJumpInst;
struct IndexedLoadInst;
struct IndexedStoreInst;
struct IndirectCallInst;
struct JumpIfInst;
struct JumpIfNotInst;
struct JumpInst;
struct LoadInst;
struct PhiInst;
struct ProgramInst;
struct ReturnInst;
struct StoreInst;
struct UnreachableInst;

struct TACVisitor
{
    virtual ~TACVisitor() {}

    virtual void visit(AssignInst* inst) = 0;
    virtual void visit(BinaryOperationInst* inst) = 0;
    virtual void visit(CallInst* inst) = 0;
    virtual void visit(ConditionalJumpInst* inst) = 0;
    virtual void visit(IndexedLoadInst* inst) = 0;
    virtual void visit(IndexedStoreInst* inst) = 0;
    virtual void visit(IndirectCallInst* inst) = 0;
    virtual void visit(JumpIfInst* inst) = 0;
    virtual void visit(JumpIfNotInst* inst) = 0;
    virtual void visit(JumpInst* inst) = 0;
    virtual void visit(LoadInst* inst) = 0;
    virtual void visit(PhiInst* inst) = 0;
    virtual void visit(ReturnInst* inst) {}
    virtual void visit(StoreInst* inst) = 0;
    virtual void visit(UnreachableInst* inst) = 0;
};

#endif
