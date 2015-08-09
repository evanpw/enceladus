#include "reg_alloc.hpp"
#include "machine_context.hpp"
#include <algorithm>
#include <deque>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stack>

RegSet& operator+=(RegSet& lhs, const RegSet& rhs)
{
    std::set_union(
        lhs.begin(), lhs.end(),
        rhs.begin(), rhs.end(),
        std::inserter(lhs, lhs.end()));

    return lhs;
}

RegSet& operator-=(RegSet& lhs, const RegSet& rhs)
{
    RegSet tmp;

    std::set_difference(
        lhs.begin(), lhs.end(),
        rhs.begin(), rhs.end(),
        std::inserter(tmp, tmp.begin()));

    lhs.swap(tmp);
    return lhs;
}

static std::ostream& operator<<(std::ostream& out, const RegSet& regSet)
{
    out << "{";

    bool first = true;
    for (Reg* reg : regSet)
    {
        if (!first)
        {
            out << ", ";
        }
        else
        {
            first = false;
        }

        out << *reg;
    }

    out << "}";

    return out;
}

RegAlloc::RegAlloc(MachineFunction* function)
: _function(function), _context(_function->context)
{
}

void RegAlloc::run()
{
    colorGraph();
    replaceRegs();

    // std::cerr << _function->name << ":" << std::endl;
    // for (MachineBB* block : _function->blocks)
    // {
    //     std::cerr << *block << ":" << std::endl;
    //     for (MachineInst* inst : block->instructions)
    //     {
    //         std::cerr << "\t" << *inst << std::endl;
    //     }
    // }
    // std::cerr << std::endl;

    spillAroundCalls();
}

void RegAlloc::spillAroundCalls()
{
    // Recompute liveness information now that we've replace virtual registers
    // and done some other rewriting
    gatherUseDef();
    computeLiveness();
    computeInterference();

    //std::cerr << _function->name << ":" << std::endl;

    // Before each call instruction, spill every live register except rsp and
    // rbp, and restore them all afterwards
    for (MachineBB* block : _function->blocks)
    {
        //std::cerr << *block << ":" << std::endl;

        // Compute live regs at the end of this block
        RegSet regs;
        for (MachineBB* succ : block->successors())
        {
            regs += _live.at(succ);
        }

        // Step through the instructions from back to front, updating live regs
        for (auto i = block->instructions.rbegin(); i != block->instructions.rend(); ++i)
        {
            MachineInst* inst = *i;

            //std::cerr << "\t" << *inst << "\t" << regs << std::endl;

            if (inst->opcode == Opcode::CALL)
            {
                std::vector<MachineInst*> saves;
                std::vector<MachineInst*> restores;
                for (Reg* liveReg : regs)
                {
                    // rbp and rsp are caller-save
                    if (liveReg == _context->rbp || liveReg == _context->rsp)
                        continue;

                    // If the function has a return value, then rax is redefined by this instruction, so
                    // we don't need to save it
                    if (!inst->outputs.empty() && liveReg == _context->rax)
                        continue;

                    StackLocation* stackVar = _function->makeStackVariable();

                    MachineInst* saveInst = new MachineInst(Opcode::MOVmd, {}, {stackVar, liveReg});
                    saves.push_back(saveInst);

                    MachineInst* restoreInst = new MachineInst(Opcode::MOVrm, {liveReg}, {stackVar});
                    restores.push_back(restoreInst);
                }

                // Insert restores "before" the current instruction, using a reverse iterator, so that
                // they end up after
                assert(saves.size() == restores.size());

                for (MachineInst* restoreInst : restores)
                {
                    block->instructions.insert(i.base(), restoreInst);
                    ++i;
                }

                // Insert save instructions "after" the current instruction
                auto next = i;
                ++next;
                for (MachineInst* saveInst : saves)
                {
                    block->instructions.insert(next.base(), saveInst);
                }
            }

            // Update liveness after checking for call instructions, because we
            // want to spill based on live-out variables, not live-in
            for (Reg* output : inst->outputs)
            {
                if (output->isRegister())
                {
                    if (regs.find(output) != regs.end())
                        regs.erase(output);
                }
            }

            for (Reg* input : inst->inputs)
            {
                if (input->isRegister())
                {
                    regs.insert(input);
                }
            }
        }
    }
}

void RegAlloc::replaceRegs()
{
    for (MachineBB* block : _function->blocks)
    {
        for (MachineInst* inst : block->instructions)
        {
            // Replace inputs
            for (size_t j = 0; j < inst->inputs.size(); ++j)
            {
                if (inst->inputs[j]->isVreg())
                    inst->inputs[j] = _context->hregs[_coloring[inst->inputs[j]]];
            }

            // Replace outputs
            for (size_t j = 0; j < inst->outputs.size(); ++j)
            {
                if (inst->outputs[j]->isVreg())
                    inst->outputs[j] = _context->hregs[_coloring[inst->outputs[j]]];
            }
        }
    }
}

void RegAlloc::gatherUseDef()
{
    _uses.clear();
    _definitions.clear();

    for (MachineBB* block : _function->blocks)
    {
        RegSet used;
        RegSet defined;

        for (MachineInst* inst : block->instructions)
        {
            for (Reg* input : inst->inputs)
            {
                if (input->isRegister() && (defined.find(input) == defined.end()))
                    used.insert(input);
            }

            for (Reg* output : inst->outputs)
            {
                if (output->isRegister())
                    defined.insert(output);
            }
        }

        _uses[block] = used;
        _definitions[block] = defined;
    }
}

void RegAlloc::computeLiveness()
{
    _live.clear();

    // Iterate until nothing changes
    while (true)
    {
        bool changed = false;

        for (MachineBB* block : _function->blocks)
        {
            RegSet regs;

            // Data flow equation:
            // live[n] = (U_{s in succ[n]}  live[s]) - def[n] + ref[n]
            for (MachineBB* succ : block->successors())
            {
                regs += _live[succ];
            }

            regs -= _definitions[block];
            regs += _uses[block];

            if (_live[block] != regs)
            {
                _live[block] = regs;
                changed = true;
            }
        }

        if (!changed)
            break;
    }
}

void RegAlloc::computeInterference()
{
    _igraph.clear();
    _precolored.clear();

    for (MachineBB* block : _function->blocks)
    {
        // Compute live regs at the end of this block
        RegSet liveOut;
        for (MachineBB* succ : block->successors())
        {
            liveOut += _live.at(succ);
        }

        // Step through the instructions from back to front, updating live regs
        for (auto i = block->instructions.rbegin(); i != block->instructions.rend(); ++i)
        {
            MachineInst* inst = *i;

            // Data flow equation:
            // live[n] = (U_{s in succ[n]}  live[s]) - def[n] + ref[n]

            RegSet newLiveOut = liveOut;

            for (Reg* output : inst->outputs)
            {
                if (output->isRegister())
                {
                    // Destinations interfere with all live-out registers
                    for (Reg* live : liveOut)
                    {
                        // Exception: if one register is moved to another, then the destination and source
                        // don't necessarily interfere
                        if (inst->opcode == Opcode::MOVrd &&
                            std::find(inst->inputs.begin(), inst->inputs.end(), live) != inst->inputs.end())
                        {
                            continue;
                        }

                        if (live != output)
                        {
                            _igraph[live].insert(output);
                            _igraph[output].insert(live);
                        }
                    }

                    if (newLiveOut.find(output) != newLiveOut.end())
                        newLiveOut.erase(output);
                }
            }

            for (Reg* input : inst->inputs)
            {
                if (input->isRegister())
                {
                    newLiveOut.insert(input);
                }
            }

            liveOut = newLiveOut;
        }
    }

    // All hardware registers are pre-colored
    for (size_t i = 0; i < 16; ++i)
    {
        Reg* hreg = _context->hregs[i];
        if (_igraph.find(hreg) != _igraph.end())
        {
            _precolored[hreg] = i;
        }
    }

    // Add an inteference edge between every pair of precolored vertices
    // (Not necessary for correct coloring, but it makes the igraph look right)
    for (auto& i : _precolored)
    {
        for (auto& j : _precolored)
        {
            if (i.first != j.first)
            {
                _igraph[i.first].insert(j.first);
                _igraph[j.first].insert(i.first);
            }

        }
    }
}

// For generating the DOT file of the colored interference graph
static std::string palette[16] =
{
    "#000000",
    "#9D9D9D",
    "#FFFFFF",
    "#BE2633",
    "#E06F8B",
    "#493C2B",
    "#A46422",
    "#EB8931",
    "#F7E26B",
    "#2F484E",
    "#44891A",
    "#A3CE27",
    "#FF00FF",
    "#005784",
    "#31A2F2",
    "#B2DCEF",
};

static std::string colorNames[16] =
{
    "rax",
    "rbx",
    "rcx",
    "rdx",
    "rsi",
    "rdi",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
    "r15",
    "rsp",
    "rbp"
};

void RegAlloc::removeFromGraph(IntGraph& graph, Reg* reg)
{
    for (Reg* other : graph[reg])
    {
        graph[other].erase(reg);
    }

    graph.erase(reg);
}

void RegAlloc::addVertexBack(IntGraph& graph, Reg* reg)
{
    for (Reg* other : _igraph.at(reg))
    {
        graph[reg].insert(other);
        graph[other].insert(reg);
    }
}

bool RegAlloc::findColorFor(const IntGraph& graph, Reg* reg)
{
    std::set<size_t> used;
    for (Reg* other : graph.at(reg))
    {
        auto i = _coloring.find(other);
        if (i != _coloring.end())
        {
            used.insert(i->second);
        }
    }

    // Handle pre-colored vertices
    auto itr = _precolored.find(reg);
    if (itr != _precolored.end())
    {
        assert(used.find(itr->second) == used.end());
        _coloring[reg] = itr->second;
        return true;
    }

    if (used.size() < AVAILABLE_COLORS)
    {
        for (size_t i = 0; i < AVAILABLE_COLORS; ++i)
        {
            if (used.find(i) == used.end())
            {
                _coloring[reg] = i;
                return true;
            }
        }

        assert(false);
    }
    else
    {
        return false;
    }
}

void RegAlloc::spillVariable(Reg* reg)
{
    VirtualRegister* vreg = dynamic_cast<VirtualRegister*>(reg);
    assert(vreg);

    std::stringstream ss;
    ss << "vreg" << vreg->id;
    StackLocation* spillLocation = _function->makeStackVariable(ss.str());

    _spilled[reg] = spillLocation;

    // Add code to spill and restore this register for each definition and use
    for (MachineBB* block : _function->blocks)
    {
        for (auto i = block->instructions.begin(); i != block->instructions.end(); ++i)
        {
            MachineInst* inst = *i;

            // Instruction uses the spilled register
            if (std::find(inst->inputs.begin(), inst->inputs.end(), reg) != inst->inputs.end())
            {
                // Load from the stack into a fresh register
                MachineOperand* newReg = _function->makeVreg(vreg->type);
                MachineInst* loadInst = new MachineInst(Opcode::MOVrm, {newReg}, {spillLocation});
                block->instructions.insert(i, loadInst);

                // Replace all uses of the spilled register with the new one
                for (size_t j = 0; j < inst->inputs.size(); ++j)
                {
                    if (inst->inputs[j] == reg)
                        inst->inputs[j] = newReg;
                }
            }

            // Instruction defines the spilled register
            if (std::find(inst->outputs.begin(), inst->outputs.end(), reg) != inst->outputs.end())
            {
                // Create a fresh register to store the result
                MachineOperand* newReg = _function->makeVreg(vreg->type);

                // Replace all uses of the spilled register with the new one
                for (size_t j = 0; j < inst->outputs.size(); ++j)
                {
                    if (inst->outputs[j] == reg)
                        inst->outputs[j] = newReg;
                }

                // Store back into the stack spill location when finished
                MachineInst* storeInst = new MachineInst(Opcode::MOVmd, {}, {spillLocation, newReg});

                auto next = i;
                ++next;
                block->instructions.insert(next, storeInst);
            }
        }
    }
}

void RegAlloc::coalesceMoves()
{
    std::unordered_map<MachineOperand*, MachineOperand*> replacements;

    for (MachineBB* block : _function->blocks)
    {

        for (auto itr = block->instructions.begin(); itr != block->instructions.end(); ++itr)
        {
            MachineInst* inst = *itr;

            if (inst->opcode == Opcode::MOVrd &&
                inst->inputs[0]->isVreg() &&
                inst->outputs[0]->isVreg())
            {
                if (inst->inputs[0] != inst->outputs[0])
                {
                    auto adjacent = _igraph.at(inst->inputs[0]);
                    if (adjacent.find(inst->outputs[0]) == adjacent.end())
                    {
                        replacements[inst->outputs[0]] = inst->inputs[0];
                    }
                }
            }
        }
    }

    for (MachineBB* block : _function->blocks)
    {
        for (MachineInst* inst : block->instructions)
        {
            // Replace inputs
            for (size_t j = 0; j < inst->inputs.size(); ++j)
            {
                if (replacements.find(inst->inputs[j]) != replacements.end())
                    inst->inputs[j] = replacements[inst->inputs[j]];
            }

            // Replace outputs
            for (size_t j = 0; j < inst->outputs.size(); ++j)
            {
                if (replacements.find(inst->outputs[j]) != replacements.end())
                    inst->outputs[j] = replacements[inst->outputs[j]];
            }
        }
    }
}

void RegAlloc::colorGraph()
{
    _spilled.clear();

    do
    {
        gatherUseDef();
        computeLiveness();
        computeInterference();

        coalesceMoves();

        gatherUseDef();
        computeLiveness();
        computeInterference();

    } while (!tryColorGraph());
}

bool RegAlloc::tryColorGraph()
{
    _coloring.clear();

    IntGraph graph = _igraph;

    std::stack<Reg*> stack;

    // While there is a vertex with degree < k, remove it from the graph and add
    // it to the stack
    while (graph.size() > _precolored.size())
    {
        bool found = false;

        for (auto& item : graph)
        {
            Reg* reg = item.first;
            auto others = item.second;

            if (_precolored.find(reg) != _precolored.end())
                continue;

            if (others.size() < AVAILABLE_COLORS)
            {
                stack.push(reg);
                removeFromGraph(graph, reg);
                found = true;
                break;
            }
        }

        // If there are no such vertices, then we may have to spill something.
        // Put off this decision until the next stage
        if (!found)
        {
            for (auto& item : graph)
            {
                Reg* reg = item.first;

                if (_precolored.find(reg) == _precolored.end())
                {
                    stack.push(reg);
                    removeFromGraph(graph, reg);
                    found = true;
                    break;
                }
            }

            assert(found);
        }
    }

    // Handle pre-colored vertices last (hardware registers)
    for (auto& item : _precolored)
    {
        Reg* hreg = item.first;

        stack.push(hreg);
        removeFromGraph(graph, hreg);
    }

    assert(graph.empty());

    // Pop off the vertices in order, add them back to the graph, and assign
    // a color
    while (!stack.empty())
    {
        Reg* reg = stack.top();
        stack.pop();

        addVertexBack(graph, reg);
        bool success = findColorFor(graph, reg);

        // If we can't color this vertex, then we must spill it and try again
        if (!success)
        {
            spillVariable(reg);
            return false;
        }
    }

    return true;
}
