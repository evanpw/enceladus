#ifndef ASM_PRINTER_HPP
#define ASM_PRINTER_HPP

#include "machine_context.hpp"
#include "machine_instruction.hpp"
#include <iostream>

class AsmPrinter
{
public:
    AsmPrinter(std::ostream& out)
    : _out(out)
    {}

    void printProgram(MachineContext* context);
    void printFunction(MachineFunction* function);

private:
    void printBlock(MachineBB* block);
    void printInstruction(MachineInst* inst);

    void printBinary(const std::string& opcode, MachineOperand* dest, MachineOperand* src);
    void printSimpleInstruction(const std::string& opcode, std::initializer_list<MachineOperand*> operands);
    void printJump(const std::string& opcode, MachineOperand* target);
    void printCallm(MachineOperand* target);
    void printMovrm(MachineOperand* dest, MachineOperand* base);
    void printMovrm(MachineOperand* dest, MachineOperand* base, MachineOperand* offset);
    void printMovmd(MachineOperand* base, MachineOperand* src);
    void printMovmd(MachineOperand* base, MachineOperand* offset, MachineOperand* src);

    void printSimpleOperand(MachineOperand* operand);

    MachineContext* _context = nullptr;
    MachineFunction* _function = nullptr;

    std::ostream& _out;

    // For generating the stack map
    size_t _callSiteCounter = 0;

    struct StackMapEntry
    {
        StackMapEntry(MachineFunction* function, size_t counter, std::set<int64_t> variables)
        : function(function), counter(counter), variables(variables)
        {}

        MachineFunction* function;
        size_t counter;
        std::set<int64_t> variables;
    };

    std::vector<StackMapEntry> _stackMap;
};

#endif