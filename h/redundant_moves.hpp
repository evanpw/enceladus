#ifndef REDUNDANT_MOVES_HPP
#define REDUNDANT_MOVES_HPP

#include "machine_instruction.hpp"

class RedundantMoves
{
public:
    RedundantMoves(MachineFunction* function);
    void run();

private:
    MachineFunction* _function;
};

#endif