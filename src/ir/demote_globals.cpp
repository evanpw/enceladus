#include "ir/demote_globals.hpp"

DemoteGlobals::DemoteGlobals(TACContext* context)
: _context(context)
{
}

Function* DemoteGlobals::getSplMain()
{
    for (Function* function : _context->functions)
    {
        if (function->name == "encmain")
            return function;
    }

    assert(false);
}

bool DemoteGlobals::isUsedOutside(Value* variable, Function* fn)
{
    for (Instruction* inst : variable->uses)
    {
        if (inst->parent->parent != fn)
            return true;
    }

    return false;
}

void DemoteGlobals::run()
{
    // Find main function
    Function* encmain = getSplMain();

    std::vector<Value*> goodGlobals;
    for (Value* global : _context->globals)
    {
        if (!isUsedOutside(global, encmain))
        {
            // Replace the global with a local of the same name
            Value* local = _context->createLocal(global->type, global->name);
            encmain->locals.push_back(local);
            encmain->replaceReferences(global, local);
        }
        else
        {
            goodGlobals.push_back(global);
        }
    }

    std::swap(_context->globals, goodGlobals);
}
