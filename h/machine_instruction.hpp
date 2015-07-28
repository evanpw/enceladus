#ifndef MACHINE_INST_HPP
#define MACHINE_INST_HPP

#include "value.hpp"
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>

// Postfix codes:
// m: indirect memory location
// i: immediate or address
// r: register
// d: either immediate or register ("direct")

enum class Opcode {
    ADD,
    AND,
    CALL,
    CMP,
    CQO,
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
    virtual ~MachineOperand() {}

    virtual void print(std::ostream& out) const = 0;

    virtual bool isVreg() const { return false; }
    virtual bool isHreg() const { return false; }
    virtual bool isRegister() const { return isVreg() || isHreg(); }
    virtual bool isAddress() const { return false; }
    virtual bool isStackLocation() const { return false; }
    virtual bool isStackParameter() const { return false; }
    virtual bool isImmediate() const { return false; }
    virtual bool isLabel() const { return false; }
};

std::ostream& operator<<(std::ostream& out, const MachineOperand& operand);

struct VirtualRegister : public MachineOperand
{
    virtual bool isVreg() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "%vreg" << id;
    }

    int64_t id;

private:
    friend class MachineFunction;

    VirtualRegister(int64_t id)
    : id(id)
    {}
};

struct HardwareRegister : public MachineOperand
{
    virtual bool isHreg() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "%" << name;
    }

    std::string name;

private:
    friend class MachineContext;

    HardwareRegister(const std::string& name)
    : name(name)
    {}
};

struct Address : public MachineOperand
{
    virtual bool isAddress() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "@" << name;
    }

    std::string name;

private:
    friend class MachineContext;

    Address(const std::string& name)
    : name(name)
    {}
};

struct StackLocation : public MachineOperand
{
    virtual bool isStackLocation() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "$" << name;
    }

    std::string name;

protected:
    friend class MachineFunction;

    StackLocation(const std::string& name)
    : name(name)
    {}
};

struct StackParameter : public StackLocation
{
    virtual bool isStackParameter() const { return true; }

    size_t index;

private:
    friend class MachineFunction;

    StackParameter(const std::string& name, size_t index)
    : StackLocation(name), index(index)
    {}
};

struct Immediate : public MachineOperand
{
    virtual bool isImmediate() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << value;
    }

    int64_t value;

private:
    friend class MachineContext;

    Immediate(int64_t value)
    : value(value)
    {}
};

struct MachineInst;
struct MachineBB : public MachineOperand
{
    ~MachineBB();

    virtual bool isLabel() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "." << id;
    }

    std::vector<MachineBB*> successors() const;

    int64_t id;
    std::list<MachineInst*> instructions;

private:
    friend class MachineFunction;

    MachineBB(int64_t id)
    : id(id)
    {}
};

struct MachineInst
{
    MachineInst(Opcode opcode,
                const std::initializer_list<MachineOperand*>&& outputs,
                const std::initializer_list<MachineOperand*>&& inputs)
    : opcode(opcode), outputs(outputs), inputs(inputs)
    {
    }

    MachineInst(Opcode opcode,
                const std::vector<MachineOperand*>&& outputs,
                const std::vector<MachineOperand*>&& inputs)
    : opcode(opcode), outputs(outputs), inputs(inputs)
    {
    }

    bool isJump() const;

    Opcode opcode;
    std::vector<MachineOperand*> outputs;
    std::vector<MachineOperand*> inputs;
};

class MachineContext;

struct MachineFunction
{
    MachineFunction(MachineContext* context, const std::string& name)
    : name(name), context(context)
    {}

    ~MachineFunction()
    {
        for (MachineBB* block : blocks)
            delete block;
    }

    std::string name;
    std::vector<MachineBB*> blocks;

    MachineContext* context;

    VirtualRegister* makeVreg();
    MachineBB* makeBlock(int64_t seqNumber);
    StackParameter* makeStackParameter(const std::string& name, size_t index);
    StackLocation* makeStackLocation(const std::string& name);

private:
    int64_t _nextVregNumber = 1;

    std::vector<std::unique_ptr<VirtualRegister>> _vregs;
    std::vector<std::unique_ptr<StackParameter>> _stackParameters;
    std::vector<std::unique_ptr<StackLocation>> _stackLocations;
};

std::ostream& operator<<(std::ostream& out, const std::vector<MachineOperand*>& operands);
std::ostream& operator<<(std::ostream& out, const MachineInst& inst);

#endif
