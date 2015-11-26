#ifndef TAC_VALIDATOR_HPP
#define TAC_VALIDATOR_HPP

#include "ir/context.hpp"

class TACValidator
{
public:
    TACValidator(TACContext* context)
    : _context(context)
    {}

    bool isValid();

private:
    // Every block ends with a terminator instruction
    bool blocksTerminated();

    // Local variables are manipulated only through store / load instructions
    bool localsGood();

    // Every temporary has a defining instruction
    bool tempsDefined();

    // Every temporary is used somewhere
    bool tempsUsed();

    // Every block successor has a corresponding predecessor, and vice-versa
    bool blockLinksGood();

    // Every block in every function is reachable from the entry block, or is
    // terminated with an UnreachableInst
    bool allBlocksReachable();

    TACContext* _context;
};

#endif
