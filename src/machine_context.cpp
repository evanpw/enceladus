#include "machine_context.hpp"

MachineContext::~MachineContext()
{
    for (size_t i = 0; i < 16; ++i)
    {
        delete hregs[i];
    }

    for (MachineFunction* function : functions)
    {
        delete function;
    }
}

Immediate* MachineContext::makeImmediate(int64_t value)
{
    auto i = _immediates.find(value);
    if (i != _immediates.end())
    {
        return i->second.get();
    }
    else
    {
        Immediate* immediate = new Immediate(value);
        _immediates.emplace(value, std::unique_ptr<Immediate>(immediate));
        return immediate;
    }
}

Address* MachineContext::makeGlobal(const std::string& name)
{
    auto i = _globals.find(name);
    if (i != _globals.end())
    {
        return i->second.get();
    }
    else
    {
        Address* address = new Address(name);
        _globals.emplace(name, std::unique_ptr<Address>(address));
        return address;
    }
}
