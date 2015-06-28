#include "tac_validator.hpp"

bool TACValidator::isValid()
{
    if (!blocksTerminated())
    {
        std::cerr << "Not all basic blocks are terminated" << std::endl;
        return false;
    }

    if (!localsGood())
    {
        std::cerr << "Not all locals are manipulated with store/load only" << std::endl;
        return false;
    }

    if (!tempsDefined())
    {
        std::cerr << "Not all temporaries have a definition" << std::endl;
    }

    return true;
}

bool TACValidator::blocksTerminated()
{
    for (Function* function : _context->functions)
    {
        for (BasicBlock* block : function->blocks)
        {
            if (!block->isTerminated())
                return false;
        }
    }

    return true;
}

bool TACValidator::localsGood()
{
    for (Function* function : _context->functions)
    {
        for (Value* value : function->locals)
        {
            if (value->definition)
            {
                return false;
            }

            for (Instruction* inst : value->uses)
            {
                if (!dynamic_cast<LoadInst*>(inst) && !dynamic_cast<StoreInst*>(inst))
                    return false;
            }
        }
    }

    return true;
}

bool TACValidator::tempsDefined()
{
    for (Function* function : _context->functions)
    {
        for (Value* value : function->temps)
        {
            if (!value->definition)
                return false;
        }
    }

    return true;
}