#ifndef MACHINE_INST_HPP
#define MACHINE_INST_HPP

#include "ir/value_type.hpp"

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    DIV,
    IDIV,
    IMUL,
    INC,
    JA,
    JAE,
    JB,
    JBE,
    JE,
    JG,
    JGE,
    JL,
    JLE,
    JMP,
    JNE,
    LEA,
    MOVmd,
    MOVrd,
    MOVrm,
    MOVSXrr,
    MOVZXrr,
    POP,
    PUSHQ,
    RET,
    SAL,
    SAR,
    SUB,
    TEST,
};

extern const char* opcodeNames[];

struct MachineOperand
{
    MachineOperand(ValueType type)
    : type(type)
    {
    }

    virtual ~MachineOperand() {}

    const ValueType type;

    virtual void print(std::ostream& out) const = 0;

    // In bits
    virtual size_t size() const { return getSize(type); }

    virtual bool isRegister() const { return false; }
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
        out << "%" << _qwordName;
    }

    std::string name(size_t size)
    {
        switch (size)
        {
            case 64:
                return _qwordName;

            case 32:
                return _dwordName;

            case 16:
                return _wordName;

            case 8:
                // Some registers aren't byte-addressable
                assert(!_byteName.empty());
                return _byteName;

            default:
                assert(false);
        }
    }

private:
    friend class MachineContext;

    HardwareRegister(const std::string& qwordName, const std::string dwordName, const std::string& wordName, const std::string& byteName)
    : _qwordName(qwordName), _dwordName(dwordName), _wordName(wordName), _byteName(byteName)
    {}

    std::string _qwordName, _dwordName, _wordName, _byteName;
};

struct VirtualRegister : public MachineOperand
{
    virtual bool isRegister() const { return true; }

    virtual void print(std::ostream& out) const
    {
        if (assignment)
        {
            out << "%" << assignment->name(getSize(type));
        }
        else
        {
            out << "%vreg" << id;
        }
    }

    const int64_t id;

    // Filled in by the register allocator
    HardwareRegister* assignment = nullptr;

private:
    friend struct MachineFunction;

    VirtualRegister(ValueType type, int64_t id)
    : MachineOperand(type), id(id)
    {}
};

static inline HardwareRegister* getAssignment(MachineOperand* operand)
{
    assert(operand->isRegister());
    return dynamic_cast<VirtualRegister*>(operand)->assignment;
}

// Constant address, like that of a global variable or function
struct Address : public MachineOperand
{
    virtual bool isAddress() const { return true; }

    virtual void print(std::ostream& out) const
    {
        out << "@" << name;
    }

    std::string name;
    bool clinkage;

private:
    friend class MachineContext;

    Address(const std::string& name, ValueType type, bool clinkage)
    : MachineOperand(type), name(name), clinkage(clinkage)
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

    std::string name;
    int64_t id = -1;

    // Filled in by stack allocator
    int64_t offset = 0;

protected:
    friend struct MachineFunction;

    StackLocation(ValueType type, const std::string& name)
    : MachineOperand(type), name(name)
    {}

    StackLocation(ValueType type, int64_t id)
    : MachineOperand(type), id(id)
    {}
};

struct StackParameter : public StackLocation
{
    virtual bool isStackParameter() const { return true; }

    size_t index;

private:
    friend struct MachineFunction;

    StackParameter(ValueType type, const std::string& name, size_t index)
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

    const int64_t value;

private:
    friend class MachineContext;

    Immediate(int64_t value, ValueType type)
    : MachineOperand(type), value(value)
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
    friend struct MachineFunction;

    MachineBB(int64_t id)
    : MachineOperand(ValueType::NonHeapAddress), id(id)
    {}
};

struct MachineInst
{
    MachineInst(Opcode opcode,
                std::vector<MachineOperand*>&& outputs,
                std::vector<MachineOperand*>&& inputs)
    : opcode(opcode), outputs(outputs), inputs(inputs)
    {
    }

    bool isJump() const;

    Opcode opcode;
    std::vector<MachineOperand*> outputs;
    std::vector<MachineOperand*> inputs;
    size_t size;
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
    StackParameter* createStackParameter(ValueType type, const std::string& name, size_t index);

    VirtualRegister* createPrecoloredReg(HardwareRegister* hreg, ValueType type);
    VirtualRegister* createVreg(ValueType type);
    MachineBB* createBlock(int64_t seqNumber);

    StackLocation* createStackVariable(ValueType type);
    StackLocation* createStackVariable(ValueType type, const std::string& name);
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
