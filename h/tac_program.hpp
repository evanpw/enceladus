#ifndef TAC_PROGRAM_HPP
#define TAC_PROGRAM_HPP

#include "function.hpp"
#include "symbol.hpp"
#include "tac_instruction.hpp"
#include "value.hpp"

#include <vector>

struct TACProgram
{
    Function* mainFunction;
    std::vector<Function*> otherFunctions;
    std::vector<std::string> externs;
    std::vector<Value*> globals;
    std::vector<std::pair<Value*, std::string>> staticStrings;
};

#endif
