#include "codegen/stack_map.hpp"
#include "codegen/machine_context.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>

StackMap::StackMap(MachineFunction* function)
: _function(function)
{
}

StackSet& operator+=(StackSet& lhs, const StackSet& rhs)
{
    std::set_union(
        lhs.begin(), lhs.end(),
        rhs.begin(), rhs.end(),
        std::inserter(lhs, lhs.end()));

    return lhs;
}

StackSet& operator-=(StackSet& lhs, const StackSet& rhs)
{
    StackSet tmp;

    std::set_difference(
        lhs.begin(), lhs.end(),
        rhs.begin(), rhs.end(),
        std::inserter(tmp, tmp.begin()));

    lhs.swap(tmp);
    return lhs;
}

static std::ostream& operator<<(std::ostream& out, const StackSet& stackSet)
{
    out << "{";

    bool first = true;
    for (int64_t location : stackSet)
    {
        if (!first)
        {
            out << ", ";
        }
        else
        {
            first = false;
        }

        out << location;
    }

    out << "}";

    return out;
}

void StackMap::run()
{
    // Gather definitions and uses
    std::unordered_map<MachineBB*, StackSet> definitions;
    std::unordered_map<MachineBB*, StackSet> uses;

    for (MachineBB* block : _function->blocks)
    {
        StackSet currentDefined;
        StackSet currentUsed;

        for (MachineInst* inst : block->instructions)
        {
            if (inst->opcode == Opcode::MOVmd)
            {
                if (inst->inputs[0]->isStackLocation())
                {
                    StackLocation* stackLocation = dynamic_cast<StackLocation*>(inst->inputs[0]);
                    if (stackLocation->type != ValueType::Reference)
                        continue;

                    assert(inst->inputs.size() == 2);
                    assert(stackLocation->offset);

                    currentDefined.insert(stackLocation->offset);
                }
            }
            else if (inst->opcode == Opcode::MOVrm)
            {
                if (inst->inputs[0]->isStackLocation())
                {
                    StackLocation* stackLocation = dynamic_cast<StackLocation*>(inst->inputs[0]);
                    if (stackLocation->type != ValueType::Reference)
                        continue;

                    assert(inst->inputs.size() == 1);
                    assert(stackLocation->offset);

                    if (currentDefined.find(stackLocation->offset) == currentDefined.end())
                    {
                        currentUsed.insert(stackLocation->offset);
                    }
                }
            }
        }

        definitions[block] = currentDefined;
        uses[block] = currentUsed;
    }

    // Determine which stack locations are live at entry
    std::unordered_map<MachineBB*, StackSet> live;

    // All stack parameters are defined at the beginning of the function
    MachineBB* entry = _function->blocks[0];
    for (size_t i = 0; i < _function->parameterCount(); ++i)
    {
        definitions[entry].insert(16 + 8 * i);
    }

    // Iterate until nothing changes
    while (true)
    {
        bool changed = false;

        for (MachineBB* block : _function->blocks)
        {
            StackSet locations;

            // Data flow equation:
            // live[n] = (U_{s in succ[n]}  live[s]) - def[n] + ref[n]
            for (MachineBB* succ : block->successors())
            {
                locations += live[succ];
            }

            locations -= definitions[block];
            locations += uses[block];

            if (live[block] != locations)
            {
                live[block] = locations;
                changed = true;
            }
        }

        if (!changed)
            break;
    }

    // For each call site, determine which stack locations are live
    for (MachineBB* block : _function->blocks)
    {
        // Compute live locations at the end of this block
        StackSet liveOut;
        for (MachineBB* succ : block->successors())
        {
            liveOut += live.at(succ);
        }

        // Step through the instructions from back to front, updating live regs
        for (auto i = block->instructions.rbegin(); i != block->instructions.rend(); ++i)
        {
            MachineInst* inst = *i;

            if (inst->opcode == Opcode::MOVmd)
            {
                if (inst->inputs[0]->isStackLocation())
                {
                    StackLocation* stackLocation = dynamic_cast<StackLocation*>(inst->inputs[0]);
                    if (stackLocation->type != ValueType::Reference)
                        continue;

                    assert(inst->inputs.size() == 2);
                    assert(stackLocation->offset);

                    liveOut.erase(stackLocation->offset);
                }
            }
            else if (inst->opcode == Opcode::MOVrm)
            {
                if (inst->inputs[0]->isStackLocation())
                {
                    StackLocation* stackLocation = dynamic_cast<StackLocation*>(inst->inputs[0]);
                    if (stackLocation->type != ValueType::Reference)
                        continue;

                    assert(inst->inputs.size() == 1);
                    assert(stackLocation->offset);

                    liveOut.insert(stackLocation->offset);
                }
            }
            else if (inst->opcode == Opcode::CALL)
            {
                _function->stackMap[inst] = liveOut;
            }
        }
    }
}
