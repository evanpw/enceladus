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

    virtual void visit(BinaryOperationInst* inst) {}
    virtual void visit(CallInst* inst) {}
    virtual void visit(ConditionalJumpInst* inst) {}
    virtual void visit(CopyInst* inst) {}
    virtual void visit(IndexedLoadInst* inst) {}
    virtual void visit(IndexedStoreInst* inst) {}
    virtual void visit(JumpIfInst* inst) {}
    virtual void visit(JumpInst* inst) {}
    virtual void visit(LoadInst* inst) {}
    virtual void visit(PhiInst* inst) {}
    virtual void visit(ReturnInst* inst) {}
    virtual void visit(StoreInst* inst) {}
    virtual void visit(TagInst* inst) {}
    virtual void visit(UnreachableInst* inst) {}
    virtual void visit(UntagInst* inst) {}
};

#endif
