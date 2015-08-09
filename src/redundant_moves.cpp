#include "redundant_moves.hpp"

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
                inst->inputs[0]->isVreg() &&
                inst->outputs[0]->isVreg() &&
                getAssignment(inst->inputs[0]) == getAssignment(inst->outputs[0]))
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
