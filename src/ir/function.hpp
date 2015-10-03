#ifndef FUNCTION_HPP
#define FUNCTION_HPP

#include "ir/basic_block.hpp"
#include "ir/value.hpp"

#include <vector>

struct Function : public GlobalValue
{
    Function(TACContext* context, const std::string& name)
    : GlobalValue(context, ValueType::NonHeapAddress, name, GlobalTag::Function)
    {}

    std::vector<BasicBlock*> blocks;

    std::vector<Value*> locals;
    std::vector<Value*> params;
    std::vector<Value*> temps;

    Value* createTemp(ValueType type);
    Value* createTemp(ValueType type, const std::string& name);
    BasicBlock* createBlock();

    void replaceReferences(Value* from, Value* to);

    void killTemp(Value* temp);

private:
    int64_t _nextSeqNumber = 0;
};

#endif
