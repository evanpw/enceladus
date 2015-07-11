#ifndef TAC_VISITOR_HPP
#define TAC_VISITOR_HPP

struct BinaryOperationInst;
struct CallInst;
struct ConditionalJumpInst;
struct CopyInst;
struct IndexedLoadInst;
struct IndexedStoreInst;
struct JumpIfInst;
struct JumpInst;
struct LoadInst;
struct PhiInst;
struct ProgramInst;
struct ReturnInst;
struct StoreInst;
struct TagInst;
struct UnreachableInst;
struct UntagInst;

struct TACVisitor
{
    virtual ~TACVisitor() {}

    virtual void visit(BinaryOperationInst* inst) = 0;
    virtual void visit(CallInst* inst) = 0;
    virtual void visit(ConditionalJumpInst* inst) = 0;
    virtual void visit(CopyInst* inst) = 0;
    virtual void visit(IndexedLoadInst* inst) = 0;
    virtual void visit(IndexedStoreInst* inst) = 0;
    virtual void visit(JumpIfInst* inst) = 0;
    virtual void visit(JumpInst* inst) = 0;
    virtual void visit(LoadInst* inst) = 0;
    virtual void visit(PhiInst* inst) = 0;
    virtual void visit(ReturnInst* inst) = 0;
    virtual void visit(StoreInst* inst) = 0;
    virtual void visit(TagInst* inst) = 0;
    virtual void visit(UnreachableInst* inst) = 0;
    virtual void visit(UntagInst* inst) = 0;
};

#endif
