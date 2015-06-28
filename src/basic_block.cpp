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

bool BasicBlock::isTerminated()
{
    return dynamic_cast<ConditionalJumpInst*>(last) ||
           dynamic_cast<JumpIfInst*>(last) ||
           dynamic_cast<JumpInst*>(last) ||
           dynamic_cast<ReturnInst*>(last) ||
           dynamic_cast<UnreachableInst*>(last);
}

std::vector<BasicBlock*> BasicBlock::getSuccessors()
{
    if (!last) return {};

    if (ConditionalJumpInst* inst = dynamic_cast<ConditionalJumpInst*>(last))
    {
        return {inst->ifTrue, inst->ifFalse};
    }
    else if (JumpIfInst* inst = dynamic_cast<JumpIfInst*>(last))
    {
        return {inst->ifTrue, inst->ifFalse};
    }
    else if (JumpInst* inst = dynamic_cast<JumpInst*>(last))
    {
        return {inst->target};
    }
    else if (dynamic_cast<ReturnInst*>(last))
    {
        return {};
    }
    else if (dynamic_cast<UnreachableInst*>(last))
    {
        assert(first == last);
        return {};
    }
    else
    {
        assert(false);
    }
}
