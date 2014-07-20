#ifndef TAC_PROGRAM_HPP
#define TAC_PROGRAM_HPP

#include "symbol.hpp"
#include "tac_instruction.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

struct TACFunction
{
    TACFunction(const std::string& name)
    : name(name)
    {}

    std::string name;
    std::vector<std::unique_ptr<TACInstruction>> instructions;

    long numberOfTemps = 0;
    std::vector<std::shared_ptr<Address>> locals;
    std::vector<std::shared_ptr<Address>> params;
    std::vector<std::shared_ptr<Address>> regParams;
    std::shared_ptr<Address> returnValue;
};

struct TACProgram
{
    TACProgram()
    : mainFunction("_main")
    {}

    TACFunction mainFunction;
    std::vector<TACFunction> otherFunctions;
    std::vector<std::string> externs;
    std::vector<std::shared_ptr<Address>> globals;
};

#endif
