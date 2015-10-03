#include "ir/function.hpp"
#include "ir/context.hpp"

#include <algorithm>
#include <sstream>

Value* Function::createTemp(ValueType type)
{
    Value* tmp = _context->createTemp(type, _nextSeqNumber++);
    temps.push_back(tmp);
    return tmp;
}

Value* Function::createTemp(ValueType type, const std::string& name)
{
    Value* tmp = _context->createTemp(type, name);
    temps.push_back(tmp);
    return tmp;
}

BasicBlock* Function::createBlock()
{
    BasicBlock* block = _context->createBlock(this, _nextSeqNumber++);
    blocks.push_back(block);
    return block;
}

void Function::replaceReferences(Value* from, Value* to)
{
    // Make a copy, because we're mutating this list
    auto uses = from->uses;

    for (Instruction* inst : uses)
    {
        inst->replaceReferences(from, to);
    }

    // There are no more references, so we can safely remove this variable
    killTemp(from);
}

void Function::killTemp(Value* temp)
{
    assert(temp->uses.empty());
    assert(!temp->definition);
    temps.erase(std::remove(temps.begin(), temps.end(), temp), temps.end());
}