#include "ir/basic_block.hpp"
#include "ir/tac_instruction.hpp"
#include <sstream>

BasicBlock::BasicBlock(TACContext* context, Function* parent, int64_t seqNumber)
: Value(context, ValueType::CodeAddress, seqNumber), parent(parent)
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

void BasicBlock::prepend(Instruction* inst)
{
    if (first)
    {
        inst->insertBefore(first);
    }
    else
    {
        first = last = inst;
    }
}

void BasicBlock::append(Instruction* inst)
{
    assert(_successors.empty());

    if (last)
    {
        inst->insertAfter(last);
    }
    else
    {
        first = last = inst;
    }

    // If we've terminated this block, add successors, and tell those blocks
    // that we're a predecessor
    std::vector<BasicBlock*> targets;
    if (getTargets(inst, targets))
    {
        for (BasicBlock* target : targets)
        {
            _successors.push_back(target);
            target->addPredecessor(this);
        }
    }
}

bool BasicBlock::isTerminated()
{
    std::vector<BasicBlock*> dummy;
    return getTargets(last, dummy);
}

bool BasicBlock::getTargets(Instruction* inst, std::vector<BasicBlock*>& targets)
{
    targets.clear();

    if (ConditionalJumpInst* branch = dynamic_cast<ConditionalJumpInst*>(inst))
    {
        targets.push_back(branch->ifTrue);
        targets.push_back(branch->ifFalse);
        return true;
    }
    else if (JumpIfInst* branch = dynamic_cast<JumpIfInst*>(inst))
    {
        targets.push_back(branch->ifTrue);
        targets.push_back(branch->ifFalse);
        return true;
    }
    else if (JumpInst* branch = dynamic_cast<JumpInst*>(inst))
    {
        targets.push_back(branch->target);
        return true;
    }
    else if (dynamic_cast<ReturnInst*>(inst) || dynamic_cast<UnreachableInst*>(inst))
    {
        return true;
    }

    return false;
}
