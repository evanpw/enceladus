#include "codegen/machine_instruction.hpp"
#include "codegen/machine_context.hpp"

#include <cassert>

const char* opcodeNames[] = {
    "ADD",
    "AND",
    "CALL",
    "CMP",
    "CQO",
    "DIV",
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

bool MachineInst::isJump() const
{
    switch (opcode)
    {
        case Opcode::JE:
        case Opcode::JG:
        case Opcode::JGE:
        case Opcode::JL:
        case Opcode::JLE:
        case Opcode::JMP:
        case Opcode::JNE:
            return true;

        default:
            return false;
    }
}

MachineBB::~MachineBB()
{
    for (MachineInst* inst : instructions)
        delete inst;
}

std::vector<MachineBB*> MachineBB::successors() const
{
    std::vector<MachineBB*> successors;

    for (auto i = instructions.rbegin(); i != instructions.rend(); ++i)
    {
        if ((*i)->isJump())
        {
            MachineBB* target = dynamic_cast<MachineBB*>((*i)->inputs[0]);
            assert(target);

            successors.push_back(target);
        }
        else
        {
            break;
        }
    }

    return successors;
}

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

MachineBB* MachineFunction::createBlock(int64_t seqNumber)
{
    MachineBB* block = new MachineBB(seqNumber);
    blocks.push_back(block);
    return block;
}

StackParameter* MachineFunction::createStackParameter(ValueType type, const std::string& name, size_t index)
{
    StackParameter* param = new StackParameter(type, name, index);
    _stackParameters.emplace_back(param);
    return param;
}

VirtualRegister* MachineFunction::createPrecoloredReg(HardwareRegister* hreg, ValueType type)
{
    VirtualRegister* vreg = new VirtualRegister(type, _nextVregNumber++);
    vreg->assignment = hreg;
    _vregs.emplace_back(vreg);
    return vreg;
}

VirtualRegister* MachineFunction::createVreg(ValueType type)
{
    VirtualRegister* vreg = new VirtualRegister(type, _nextVregNumber++);
    _vregs.emplace_back(vreg);
    return vreg;
}

StackLocation* MachineFunction::createStackVariable(ValueType type)
{
    StackLocation* location = new StackLocation(type, _nextStackVar++);
    _stackVariables.emplace_back(location);
    return location;
}

StackLocation* MachineFunction::createStackVariable(ValueType type, const std::string& name)
{
    StackLocation* location = new StackLocation(type, name);
    _stackVariables.emplace_back(location);
    return location;
}
