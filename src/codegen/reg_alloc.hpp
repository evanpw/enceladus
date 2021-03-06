#ifndef REG_ALLOC_HPP
#define REG_ALLOC_HPP

#include "codegen/machine_instruction.hpp"

#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A set of registers
typedef MachineOperand Reg;
typedef std::set<Reg*> RegSet;
RegSet& operator+=(RegSet& lhs, const RegSet& rhs);
RegSet& operator-=(RegSet& lhs, const RegSet& rhs);

// An interference graph
typedef std::unordered_map<Reg*, std::unordered_set<Reg*>> IntGraph;

// An assignment of a color to each register
typedef std::unordered_map<Reg*, size_t> Coloring;

// Postcondition: All VirtualRegister operands are assigned
class RegAlloc
{
public:
    RegAlloc(MachineFunction* function);
    void run();

private:
    MachineFunction* _function;
    MachineContext* _context;

    // Never allocate rsp and rbp
    static constexpr size_t AVAILABLE_COLORS = 14;

    // For each basic block, determine the registers that are:
    // 1) given a value in that block
    // 2) used before they are defined
    void gatherUseDef();
    std::unordered_map<MachineBB*, RegSet> _definitions;    // (1)
    std::unordered_map<MachineBB*, RegSet> _uses;           // (2)

    // For each basic block, the registers which are live upon entry
    void computeLiveness();
    std::unordered_map<MachineBB*, RegSet> _live;

    void getPrecolored();
    size_t colorOfHreg(HardwareRegister* hreg);
    Coloring _precolored;

    // For each register, which registers are simultaneously live?
    void computeInterference();
    IntGraph _igraph;

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
    void assignRegs();
    //void replaceRegs();

    // Combine live ranges that are related by a move instruction and which
    // don't interfere
    void coalesceMoves();

    // Spill all callee-save registers at call sites
    void spillAroundCalls();
};

#endif