#ifndef MACHINE_INST_HPP
#define MACHINE_INST_HPP

#include "value.hpp"
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
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

enum OperandType {MaybeReference, NotReference};

static inline const char* operandTypeString(OperandType type)
{
    switch(type)
    {
        case MaybeReference:
            return "MaybeReference";

        case NotReference:
            return "NotReference";
    }

    assert(false);
}

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

struct HardwareRegister
{
    void print(std::ostream& out) const
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

struct VirtualRegister : public MachineOperand
{
    virtual bool isVreg() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "%vreg" << id;
    }

    const OperandType type;
    const int64_t id;

    // Filled in by the register allocator
    HardwareRegister* assignment = nullptr;

private:
    friend class MachineFunction;

    VirtualRegister(OperandType type, int64_t id)
    : type(type), id(id)
    {}
};

static inline HardwareRegister* getAssignment(MachineOperand* operand)
{
    assert(operand->isVreg());
    return dynamic_cast<VirtualRegister*>(operand)->assignment;
}

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
        if (id == -1)
            out << "$" << name;
        else
            out << "$" << id;
    }

    const OperandType type;

    std::string name;
    int64_t id = -1;

    // Filled in by stack allocator
    int64_t offset = 0;

protected:
    friend class MachineFunction;

    StackLocation(OperandType type, const std::string& name)
    : type(type), name(name)
    {}

    StackLocation(OperandType type, int64_t id)
    : type(type), id(id)
    {}
};

struct StackParameter : public StackLocation
{
    virtual bool isStackParameter() const { return true; }

    size_t index;

private:
    friend class MachineFunction;

    StackParameter(OperandType type, const std::string& name, size_t index)
    : StackLocation(type, name), index(index)
    {
        offset = 16 + 8 * index;
    }
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
    : context(context), name(name)
    {}

    ~MachineFunction()
    {
        for (MachineBB* block : blocks)
            delete block;
    }

    MachineContext* context;

    std::string name;
    std::vector<MachineBB*> blocks;

    std::unordered_map<MachineInst*, std::set<int64_t>> stackMap;

    size_t parameterCount() const { return _stackParameters.size(); }
    StackParameter* getParameter(size_t i) { return _stackParameters.at(i).get(); }
    StackParameter* makeStackParameter(OperandType type, const std::string& name, size_t index);

    VirtualRegister* makePrecoloredReg(HardwareRegister* hreg, OperandType type);
    VirtualRegister* makeVreg(OperandType type);
    MachineBB* makeBlock(int64_t seqNumber);

    StackLocation* makeStackVariable(OperandType type);
    StackLocation* makeStackVariable(OperandType type, const std::string& name);
    size_t stackVariableCount() const { return _stackVariables.size(); }
    StackLocation* getStackVariable(size_t i) { return _stackVariables.at(i).get(); }

private:
    int64_t _nextVregNumber = 1;
    std::vector<std::unique_ptr<VirtualRegister>> _vregs;

    std::vector<std::unique_ptr<StackParameter>> _stackParameters;

    int64_t _nextStackVar = 1;
    std::vector<std::unique_ptr<StackLocation>> _stackVariables;
};

std::ostream& operator<<(std::ostream& out, const std::vector<MachineOperand*>& operands);
std::ostream& operator<<(std::ostream& out, const MachineInst& inst);

#endif
