#ifndef FUNCTION_HPP
#define FUNCTION_HPP

#include "basic_block.hpp"
#include "value.hpp"
#include <vector>

struct Function : public GlobalValue
{
    Function(TACContext* context, const std::string& name)
    : GlobalValue(context, ValueType::CodeAddress, name, GlobalTag::Function)
    {}

    std::vector<BasicBlock*> blocks;

    std::vector<Value*> locals;
    std::vector<Value*> params;
    std::vector<Value*> temps;

    Value* makeTemp(ValueType type);
    Value* makeTemp(ValueType type, const std::string& name);
    BasicBlock* makeBlock();

    void replaceReferences(Value* from, Value* to);

    void killTemp(Value* temp);

private:
    int64_t _nextSeqNumber = 0;
};

#endif
