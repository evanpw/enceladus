#include "codegen/machine_context.hpp"

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

Immediate* MachineContext::createImmediate(int64_t value, ValueType type)
{
    Immediate* immediate = new Immediate(value, type);
    _immediates.emplace_back(immediate);

    return immediate;
}

Address* MachineContext::createGlobal(const std::string& name, bool clinkage)
{
    auto i = _globals.find(name);
    if (i != _globals.end())
    {
        return i->second.get();
    }
    else
    {
        Address* address = new Address(name, clinkage);
        _globals.emplace(name, std::unique_ptr<Address>(address));
        return address;
    }
}
