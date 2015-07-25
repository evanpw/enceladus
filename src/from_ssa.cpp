#include "from_ssa.hpp"

#include "basic_block.hpp"
#include "tac_instruction.hpp"
#include "value.hpp"

FromSSA::FromSSA(Function* function)
: _function(function)
{
}

void FromSSA::run()
{
    for (auto& local : _function->locals)
    {
        assert(local->uses.empty());
    }
    _function->locals.clear();


    for (BasicBlock* block : _function->blocks)
    {
        Instruction* inst = block->first;
        while (inst != nullptr)
        {
            if (PhiInst* phi = dynamic_cast<PhiInst*>(inst))
            {
                // Each predecessor will copy into this variable, and then the
                // phi node will be replaced with a copy from this variable to
                // the phi destination. This is usually overkill (compared to
                // just a single copy in each predecessor), but it helps avoid
                // problems in some corner cases, and the extra copies will
                // hopefully be coalesced during register allocation.
                Value* temp = _function->makeTemp();

                for (auto& source : phi->sources())
                {
                    BasicBlock* pred = source.first;
                    Value* value = source.second;

                    CopyInst* copy = new CopyInst(temp, value);
                    copy->insertBefore(pred->last);
                }

                // And delete the phi node
                inst = inst->next;
                phi->replaceWith(new CopyInst(phi->dest, temp));
            }
            else
            {
                break;
            }
        }
    }
}
