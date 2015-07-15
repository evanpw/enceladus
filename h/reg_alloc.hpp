#ifndef REG_ALLOC_HPP
#define REG_ALLOC_HPP

#include "machine_instruction.hpp"
#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A set of registers
typedef MachineOperand Reg;
typedef std::set<Reg*> RegSet;
std::ostream& operator<<(std::ostream& out, const RegSet& regs);
RegSet& operator+=(RegSet& lhs, const RegSet& rhs);
RegSet& operator-=(RegSet& lhs, const RegSet& rhs);

// An interference graph
typedef std::unordered_map<Reg*, std::unordered_set<Reg*>> IntGraph;

// An assignment of a color to each register
typedef std::unordered_map<Reg*, size_t> Coloring;

class RegAlloc
{
public:
    void run(MachineFunction* function);
    void dumpGraph() const;

private:
    MachineFunction* _function;
    MachineContext* _context;

    static constexpr size_t AVAILABLE_COLORS = 6;

    // For debugging: show live variables at each machine instruction
    void dumpLiveness() const;

    // For each basic block, the registers which are given a value in that block
    void gatherDefinitions();
    std::unordered_map<MachineBB*, RegSet> _definitions;

    // For each basic block, the registers which are used before they are defined
    void gatherUses();
    std::unordered_map<MachineBB*, RegSet> _uses;

    // For each basic block, the registers which are live upon entry
    void computeLiveness();
    std::unordered_map<MachineBB*, RegSet> _live;

    // For each register, which registers are simultaneously live?
    void computeInterference();
    IntGraph _igraph;
    Coloring _precolored;

    // An assignment to each register of a color (< AVAILABLE_COLORS) such that
    // no two registers which interfere are assigned the same color
    Coloring _coloring;

    std::unordered_map<Reg*, StackLocation*> _spilled;

    // Graph coloring
    void removeFromGraph(IntGraph& graph, Reg* reg);
    void addVertexBack(IntGraph& graph, Reg* reg);
    bool findColorFor(const IntGraph& graph, Reg* reg);
    void spillVariable(Reg* reg);
    bool tryColorGraph();

    // Choose hardware registers for each virtual register
    void colorGraph();

    // Rewrite the function to replace virtual registers with hardware registers
    void replaceRegs();

    // Assign an explicit location on the stack for each StackLocation and
    // rewrite instructions to use those locations
    std::unordered_map<StackLocation*, Immediate*> _stackOffsets;
    int64_t _currentOffset = 0;
    void assignStackLocations();
    Immediate* getStackOffset(MachineOperand* operand);

    // In the entry block, adjust rsp to allocate enough memory for all spilled
    // variables
    void allocateStack();

    // Spill all callee-save registers at call sites
    void spillAroundCalls();
};

#endif