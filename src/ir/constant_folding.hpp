#ifndef CONSTANT_FOLDING_HPP
#define CONSTANT_FOLDING_HPP

#include "ir/context.hpp"
#include "ir/function.hpp"
#include "ir/tac_visitor.hpp"

class ConstantFolding : public TACVisitor
{
public:
    ConstantFolding(Function* function);
    void run();

    virtual void visit(CopyInst* inst);
    virtual void visit(BinaryOperationInst* inst);

private:
    Function* _function;
    TACContext* _context;
};

#endif
