#ifndef FUNCTION_HPP
#define FUNCTION_HPP

#include "basic_block.hpp"
#include "value.hpp"
#include <vector>

struct Function : public GlobalValue
{
    Function(const std::string& name)
    : GlobalValue(name, GlobalTag::Function)
    {}

    std::vector<BasicBlock*> blocks;

    std::vector<Value*> locals;
    std::vector<Value*> params;
    std::vector<Value*> temps;
};

#endif
