#ifndef CONSTANT_FOLDING_HPP
#define CONSTANT_FOLDING_HPP

#include "context.hpp"
#include "function.hpp"
#include "tac_visitor.hpp"

class ConstantFolding : public TACVisitor
{
public:
    ConstantFolding(Function* function);
    void run();

    // Handled
    virtual void visit(CopyInst* inst);
    virtual void visit(BinaryOperationInst* inst);
    virtual void visit(TagInst* inst);
    virtual void visit(UntagInst* inst);

    // Unhandled
    virtual void visit(CallInst* inst) {}
    virtual void visit(ConditionalJumpInst* inst) {}
    virtual void visit(IndexedLoadInst* inst) {}
    virtual void visit(IndexedStoreInst* inst) {}
    virtual void visit(JumpIfInst* inst) {}
    virtual void visit(JumpInst* inst) {}
    virtual void visit(LoadInst* inst) {}
    virtual void visit(PhiInst* inst) {}
    virtual void visit(ReturnInst* inst) {}
    virtual void visit(StoreInst* inst) {}
    virtual void visit(UnreachableInst* inst) {}

private:
    Function* _function;
    TACContext* _context;
};

#endif
