#ifndef DEMOTE_GLOBALS
#define DEMOTE_GLOBALS

#include "ir/context.hpp"

// Demote globals which are only ever used in splmain to local variables
class DemoteGlobals
{
public:
    DemoteGlobals(TACContext* context);
    void run();

private:
    Function* getSplMain();
    bool isUsedOutside(Value* variable, Function* fn);

    TACContext* _context;
};

#endif
