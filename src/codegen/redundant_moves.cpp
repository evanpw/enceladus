#include "codegen/redundant_moves.hpp"

RedundantMoves::RedundantMoves(MachineFunction* function)
: _function(function)
{
}

void RedundantMoves::run()
{
    for (MachineBB* mbb : _function->blocks)
    {
        for (auto itr = mbb->instructions.begin(); itr != mbb->instructions.end();)
        {
            MachineInst* inst = *itr;

            if (inst->opcode == Opcode::MOVrd &&
                inst->inputs[0]->isRegister() &&
                inst->outputs[0]->isRegister() &&
                getAssignment(inst->inputs[0]) == getAssignment(inst->outputs[0]) &&
                inst->inputs[0]->size() == inst->outputs[0]->size())
            {
                itr = mbb->instructions.erase(itr);
                delete inst;
                continue;
            }
            else
            {
                ++itr;
            }
        }
    }
}
