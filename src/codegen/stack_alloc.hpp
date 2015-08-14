#ifndef STACK_ALLOC_HPP
#define STACK_ALLOC_CPP

#include "codegen/machine_instruction.hpp"

class StackAlloc
{
public:
    StackAlloc(MachineFunction* function);
    void run();

private:
    MachineFunction* _function;
    MachineContext* _context;
};

#endif