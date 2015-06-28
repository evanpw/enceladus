#ifndef TAC_VALIDATOR_HPP
#define TAC_VALIDATOR_HPP

#include "context.hpp"

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

    TACContext* _context;
};

#endif