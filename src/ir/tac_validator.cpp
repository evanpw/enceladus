#include "ir/tac_validator.hpp"

#include <algorithm>

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
        return false;
    }

    if (!blockLinksGood())
    {
        std::cerr << "Not all links between blocks are bidirectional" << std::endl;
        return false;
    }

    if (!allBlocksReachable())
    {
        std::cerr << "Not all basic blocks are reachable" << std::endl;
        //return false;
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

bool TACValidator::blockLinksGood()
{
    for (Function* function : _context->functions)
    {
        for (BasicBlock* block : function->blocks)
        {
            const std::vector<BasicBlock*> successors = block->successors();
            const std::vector<BasicBlock*> predecessors = block->predecessors();

            for (BasicBlock* successor : successors)
            {
                const std::vector<BasicBlock*> sp = successor->predecessors();

                if (std::find(sp.begin(), sp.end(), block) == sp.end())
                    return false;
            }

            for (BasicBlock* predecessor : predecessors)
            {
                const std::vector<BasicBlock*> ps = predecessor->successors();

                if (std::find(ps.begin(), ps.end(), block) == ps.end())
                    return false;
            }
        }
    }

    return true;
}

// Helper function for allBlocksReachable
static void gatherBlocks(BasicBlock* block, std::set<BasicBlock*>& reached)
{
    if (reached.find(block) != reached.end())
        return;

    reached.insert(block);

    for (BasicBlock* successor : block->successors())
    {
        gatherBlocks(successor, reached);
    }
}

bool TACValidator::allBlocksReachable()
{
    for (Function* function : _context->functions)
    {
        std::set<BasicBlock*> allBlocks;
        allBlocks.insert(function->blocks.begin(), function->blocks.end());

        std::set<BasicBlock*> reachableBlocks;
        gatherBlocks(function->blocks[0], reachableBlocks);

        std::set<BasicBlock*> unreachableBlocks;
        std::set_difference(
            allBlocks.begin(), allBlocks.end(),
            reachableBlocks.begin(), reachableBlocks.end(),
            std::inserter(unreachableBlocks, unreachableBlocks.begin()));

        for (BasicBlock* unreachable : unreachableBlocks)
        {
            if (!dynamic_cast<UnreachableInst*>(unreachable->last))
                return false;
        }
    }

    return true;
}
