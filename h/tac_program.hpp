#ifndef TAC_PROGRAM_HPP
#define TAC_PROGRAM_HPP

#include "symbol.hpp"
#include "tac_instruction.hpp"

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

struct TACFunction
{
    TACFunction(const std::string& name)
    : name(name)
    {}

    ~TACFunction()
    {
        delete instructions;
    }

    TACFunction(TACFunction&& rhs)
    {
        name = rhs.name;
        instructions = rhs.instructions;
        rhs.instructions = 0;
        numberOfTemps = rhs.numberOfTemps;
        locals = std::move(rhs.locals);
        params = std::move(rhs.params);
        regParams = std::move(rhs.regParams);
        returnValue = std::move(rhs.returnValue);
    }

    std::string name;
    TACInstruction* instructions = nullptr;

    long numberOfTemps = 0;
    std::vector<std::shared_ptr<Address>> locals;
    std::vector<std::shared_ptr<Address>> params;
    std::vector<std::shared_ptr<Address>> regParams;
    std::shared_ptr<Address> returnValue;
};

struct TACProgram
{
    TACProgram()
    : mainFunction("main")
    {}

    TACFunction mainFunction;
    std::vector<TACFunction> otherFunctions;
    std::vector<std::string> externs;
    std::vector<std::shared_ptr<Address>> globals;
    std::vector<std::pair<std::shared_ptr<Address>, std::string>> staticStrings;
};

#endif
