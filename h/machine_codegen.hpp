#ifndef MACHINE_CODEGEN_HPP
#define MACHINE_CODEGEN_HPP

#include "basic_block.hpp"
#include "function.hpp"
#include "machine_instruction.hpp"
#include "tac_visitor.hpp"
#include "value.hpp"
#include <initializer_list>
#include <unordered_map>
#include <utility>

class MachineCodeGen : public TACVisitor
{
public:
    MachineCodeGen(Function* function);

    virtual void visit(BinaryOperationInst* inst);
    virtual void visit(CallInst* inst);
    virtual void visit(ConditionalJumpInst* inst);
    virtual void visit(CopyInst* inst);
    virtual void visit(IndexedLoadInst* inst);
    virtual void visit(IndexedStoreInst* inst);
    virtual void visit(JumpIfInst* inst);
    virtual void visit(JumpInst* inst);
    virtual void visit(LoadInst* inst);
    virtual void visit(PhiInst* inst);
    virtual void visit(ReturnInst* inst);
    virtual void visit(StoreInst* inst);
    virtual void visit(TagInst* inst);
    virtual void visit(UnreachableInst* inst);
    virtual void visit(UntagInst* inst);

    std::vector<MachineInst*> instructions;

private:
    MachineBB* _currentBlock = nullptr;

    void emit(Opcode opcode,
              std::initializer_list<MachineOperand*> outputs,
              std::initializer_list<MachineOperand*> inputs)
    {
        MachineInst* inst = new MachineInst(opcode, std::move(outputs), std::move(inputs));
        std::cerr << "\t" << *inst << std::endl;
        _currentBlock->instructions.push_back(inst);
    }

    // Convert an IR Value to a machine operand
    MachineOperand* getOperand(Value* value);

    int64_t _nextVregNumber = 1;
    std::unordered_map<Value*, VirtualRegister*> _vregs;

    // Maps IR basic blocks to machine basic blocks
    std::unordered_map<BasicBlock*, MachineBB*> _blocks;
    MachineBB* getBlock(BasicBlock* block);

    // For convenient access
    HardwareRegister* rax;
    HardwareRegister* rdx;
    HardwareRegister* rsp;
};

#endif
