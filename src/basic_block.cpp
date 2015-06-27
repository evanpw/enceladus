#include "basic_block.hpp"
#include "tac_instruction.hpp"
#include <sstream>

BasicBlock::BasicBlock(int64_t seqNumber)
: seqNumber(seqNumber)
{
}

BasicBlock::~BasicBlock()
{
    Instruction* p = first;
    while (p)
    {
        Instruction* tmp = p;
        p = tmp->next;

        delete tmp;
    }
}

std::string BasicBlock::str() const
{
    if (name.length() > 0)
    {
        return "label " + name;
    }
    else
    {
        std::stringstream ss;
        ss << "label ." << seqNumber;
        return ss.str();
    }
}

void BasicBlock::append(Instruction* inst)
{
    if (last)
    {
        inst->insertAfter(last);
        last = inst;
    }
    else
    {
        first = last = inst;
    }
}

std::vector<BasicBlock*> BasicBlock::getSuccessors()
{
    if (!last) return {};

    if (TACConditionalJump* inst = dynamic_cast<TACConditionalJump*>(last))
    {
        return {inst->ifTrue, inst->ifFalse};
    }
    else if (TACJumpIf* inst = dynamic_cast<TACJumpIf*>(last))
    {
        return {inst->ifTrue, inst->ifFalse};
    }
    else if (TACJump* inst = dynamic_cast<TACJump*>(last))
    {
        return {inst->target};
    }
    else if (dynamic_cast<TACReturn*>(last))
    {
        return {};
    }
    else
    {
        assert(false);
    }
}
