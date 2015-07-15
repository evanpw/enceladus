#include "machine_context.hpp"

MachineContext::~MachineContext()
{
    for (size_t i = 0; i < 16; ++i)
    {
        delete hregs[i];
    }
}