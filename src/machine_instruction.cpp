#include "machine_instruction.hpp"

const char* opcodeNames[] = {
    "ADD",
    "AND",
    "CALLi",
    "CALLm",
    "CMP",
    "CQO",
    "DEC",
    "IDIV",
    "IMUL",
    "INC",
    "JE",
    "JG",
    "JGE",
    "JL",
    "JLE",
    "JMP",
    "JNE",
    "LEA",
    "MOVrd",
    "MOVrm",
    "MOVmd",
    "POP",
    "PUSH",
    "RET",
    "SAL",
    "SAR",
    "SUB",
    "TEST",
};

std::ostream& operator<<(std::ostream& out, const MachineOperand& operand)
{
    operand.print(out);
    return out;
}

std::ostream& operator<<(std::ostream& out, const std::vector<MachineOperand*>& operands)
{
    if (operands.empty())
    {
        out << "{}";
        return out;
    }

    out << *operands[0];
    for (size_t i = 1; i < operands.size(); ++i)
    {
        out << ", " << *operands[i];
    }

    return out;
}

std::ostream& operator<<(std::ostream& out, const MachineInst& inst)
{
    out << inst.outputs
        << " = "
        << opcodeNames[static_cast<int>(inst.opcode)]
        << " "
        << inst.inputs;

    return out;
}