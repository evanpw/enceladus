#include "ir/kill_dead_values.hpp"

KillDeadValues::KillDeadValues(Function* function)
: _function(function), _context(function->context())
{
}

void KillDeadValues::run()
{
    while (true)
    {
        _changed = false;

        for (BasicBlock* block : _function->blocks)
        {
            Instruction* inst = block->first;

            while (inst)
            {
                Instruction* next = inst->next;
                inst->accept(this);
                inst = next;
            }
        }

        if (!_changed)
            break;
    }
}

void KillDeadValues::visit(BinaryOperationInst* inst)
{
    if (inst->dest->uses.empty())
    {
        inst->removeFromParent();
        _changed = true;
    }
}

void KillDeadValues::visit(CopyInst* inst)
{
    if (inst->dest->uses.empty())
    {
        inst->removeFromParent();
        _changed = true;
    }
}

void KillDeadValues::visit(IndexedLoadInst* inst)
{
    if (inst->lhs->uses.empty())
    {
        inst->removeFromParent();
        _changed = true;
    }
}

void KillDeadValues::visit(LoadInst* inst)
{
    if (inst->dest->uses.empty())
    {
        inst->removeFromParent();
        _changed = true;
    }
}

void KillDeadValues::visit(PhiInst* inst)
{
    if (inst->dest->uses.empty())
    {
        inst->removeFromParent();
        _changed = true;
    }
}
