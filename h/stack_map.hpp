#ifndef STACK_MAP_HPP
#define STACK_MAP_HPP

#include "machine_instruction.hpp"

typedef std::set<int64_t> StackSet;
StackSet& operator+=(StackSet& lhs, const StackSet& rhs);
StackSet& operator-=(StackSet& lhs, const StackSet& rhs);

// Create a table listing which stack variables are live at each call site
class StackMap
{
public:
    StackMap(MachineFunction* function);
    void run();

private:
    MachineFunction* _function;
    MachineContext* _context;
};

#endif