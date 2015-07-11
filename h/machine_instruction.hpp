#ifndef MACHINE_INST_HPP
#define MACHINE_INST_HPP

#include "value.hpp"
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>

// Postfix codes:
// m: indirect memory location
// i: immediate or address
// r: register
// d: either immediate or register ("direct")

enum class Opcode {
    ADD,
    AND,
    CALLi,
    CALLm,
    CMP,
    CQO,
    DEC,
    IDIV,
    IMUL,
    INC,
    JE,
    JG,
    JGE,
    JL,
    JLE,
    JMP,
    JNE,
    LEA,
    MOVrd,
    MOVrm,
    MOVmd,
    POP,
    PUSH,
    RET,
    SAL,
    SAR,
    SUB,
    TEST,
};

extern const char* opcodeNames[];

struct MachineOperand
{
    virtual void print(std::ostream& out) const = 0;

    virtual bool isVreg() const { return false; }
    virtual bool isHreg() const { return false; }
    virtual bool isRegister() const { return isVreg() || isHreg(); }
    virtual bool isAddress() const { return false; }
    virtual bool isStackLocation() const { return false; }
    virtual bool isImmediate() const { return false; }
    virtual bool isLabel() const { return false; }
};

std::ostream& operator<<(std::ostream& out, const MachineOperand& operand);

struct VirtualRegister : public MachineOperand
{
    VirtualRegister(int64_t id)
    : id(id)
    {}

    virtual bool isVreg() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "%vreg" << id;
    }

    int64_t id;
};

struct HardwareRegister : public MachineOperand
{
    HardwareRegister(const std::string& name)
    : name(name)
    {}

    virtual bool isHreg() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "%" << name;
    }

    std::string name;
};

struct Address : public MachineOperand
{
    Address(const std::string& name)
    : name(name)
    {}

    virtual bool isAddress() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "@" << name;
    }

    std::string name;
};

struct StackLocation : public MachineOperand
{
    StackLocation(const std::string& name)
    : name(name)
    {}

    virtual bool isStackLocation() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "$" << name;
    }

    std::string name;
};

struct Immediate : public MachineOperand
{
    Immediate(int64_t value)
    : value(value)
    {}

    virtual bool isImmediate() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << value;
    }

    int64_t value;
};

struct MachineInst;
struct MachineBB : public MachineOperand
{
    MachineBB(int64_t id)
    : id(id)
    {}

    virtual bool isLabel() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "." << id;
    }

    int64_t id;
    std::vector<MachineInst*> instructions;
};

struct MachineInst
{
    MachineInst(Opcode opcode,
                const std::initializer_list<MachineOperand*>&& outputs,
                const std::initializer_list<MachineOperand*>&& inputs)
    : opcode(opcode), outputs(outputs), inputs(inputs)
    {
    }

    Opcode opcode;
    std::vector<MachineOperand*> outputs;
    std::vector<MachineOperand*> inputs;
};

std::ostream& operator<<(std::ostream& out, const std::vector<MachineOperand*>& operands);

std::ostream& operator<<(std::ostream& out, const MachineInst& inst);

#endif