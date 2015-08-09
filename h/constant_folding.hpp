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

    virtual void visit(CopyInst* inst);
    virtual void visit(BinaryOperationInst* inst);
    virtual void visit(TagInst* inst);
    virtual void visit(UntagInst* inst);

private:
    Function* _function;
    TACContext* _context;
};

#endif
