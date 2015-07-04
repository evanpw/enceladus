#include "function.hpp"
#include "context.hpp"
#include <algorithm>
#include <sstream>

Value* Function::makeTemp()
{
    Value* tmp = _context->makeTemp(_nextSeqNumber++);
    temps.push_back(tmp);
    return tmp;
}

Value* Function::makeTemp(const std::string& name)
{
    Value* tmp = _context->makeTemp(name);
    temps.push_back(tmp);
    return tmp;
}

BasicBlock* Function::makeBlock()
{
    BasicBlock* block = _context->makeBlock(_nextSeqNumber++);
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
    assert(from->uses.empty());
    assert(!from->definition);
    temps.erase(std::remove(temps.begin(), temps.end(), from), temps.end());
}