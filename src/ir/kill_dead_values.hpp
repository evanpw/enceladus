#ifndef KILL_DEAD_VALUES
#define KILL_DEAD_VALUES

#include "ir/context.hpp"
#include "ir/function.hpp"
#include "ir/tac_visitor.hpp"

class KillDeadValues : TACVisitor
{
public:
    KillDeadValues(Function* function);

    void run();

    virtual void visit(BinaryOperationInst* inst);
    virtual void visit(CopyInst* inst);
    virtual void visit(IndexedLoadInst* inst);
    virtual void visit(LoadInst* inst);
    virtual void visit(PhiInst* inst);

private:
    Function* _function;
    TACContext* _context;

    bool _changed;
};

#endif
