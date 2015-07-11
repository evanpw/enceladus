#include "machine_codegen.hpp"
#include "tac_instruction.hpp"
#include "value.hpp"

MachineCodeGen::MachineCodeGen(Function* function)
{
    rax = new HardwareRegister("rax");
    rdx = new HardwareRegister("rdx");
    rsp = new HardwareRegister("rsp");

    for (BasicBlock* irBlock : function->blocks)
    {
        _currentBlock = getBlock(irBlock);
        std::cerr << irBlock->str() << std::endl;

        for (Instruction* inst = irBlock->first; inst != nullptr; inst = inst->next)
        {
            inst->accept(this);
        }
    }
}

MachineOperand* MachineCodeGen::getOperand(Value* value)
{
    if (ConstantInt* constInt = dynamic_cast<ConstantInt*>(value))
    {
        // TODO: Intern this
        return new Immediate(constInt->value);
    }
    else if (GlobalValue* global = dynamic_cast<GlobalValue*>(value))
    {
        return new Address(global->name);
    }
    else if (dynamic_cast<LocalValue*>(value))
    {
        // These should have all been converted to temporaries
        assert(false);
    }
    else if (Argument* arg = dynamic_cast<Argument*>(value))
    {
        return new StackLocation(arg->name);
    }
    else if (BasicBlock* block = dynamic_cast<BasicBlock*>(value))
    {
        return getBlock(block);
    }
    else /* temporary */
    {
        auto i = _vregs.find(value);
        if (i != _vregs.end())
        {
            return i->second;
        }

        VirtualRegister* vreg = new VirtualRegister(_nextVregNumber++);
        _vregs[value] = vreg;

        return vreg;
    }
}

MachineBB* MachineCodeGen::getBlock(BasicBlock* block)
{
    auto i = _blocks.find(block);
    if (i != _blocks.end())
    {
        return i->second;
    }

    MachineBB* mbb = new MachineBB(block->seqNumber);
    _blocks[block] = mbb;

    return mbb;
}

void MachineCodeGen::visit(BinaryOperationInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* lhs = getOperand(inst->lhs);
    MachineOperand* rhs = getOperand(inst->rhs);
    assert(dest->isRegister());
    assert(lhs->isRegister() || lhs->isImmediate());
    assert(rhs->isRegister() || rhs->isImmediate());

    if (inst->op == BinaryOperation::ADD)
    {
        emit(Opcode::MOVrd, {dest}, {lhs});
        emit(Opcode::ADD, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::SUB)
    {
        emit(Opcode::MOVrd, {dest}, {lhs});
        emit(Opcode::SUB, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::MUL)
    {
        emit(Opcode::MOVrd, {dest}, {lhs});
        emit(Opcode::IMUL, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::AND)
    {
        emit(Opcode::MOVrd, {dest}, {lhs});
        emit(Opcode::AND, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::SHL)
    {
        assert(rhs->isImmediate());
        int64_t value = dynamic_cast<Immediate*>(rhs)->value;
        assert(value < 32);

        emit(Opcode::MOVrd, {dest}, {lhs});
        emit(Opcode::SAL, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::SHR)
    {
        assert(rhs->isImmediate());
        int64_t value = dynamic_cast<Immediate*>(rhs)->value;
        assert(value < 32);

        emit(Opcode::MOVrd, {dest}, {lhs});
        emit(Opcode::SAR, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::DIV)
    {
        emit(Opcode::MOVrd, {rax}, {lhs});
        emit(Opcode::CQO, {rdx}, {rax});
        emit(Opcode::IDIV, {rdx, rax}, {rdx, rax, rhs});
        emit(Opcode::MOVrd, {dest}, {rax});
    }
    else if (inst->op == BinaryOperation::MOD)
    {
        emit(Opcode::MOVrd, {rax}, {lhs});
        emit(Opcode::CQO, {rdx}, {rax});
        emit(Opcode::IDIV, {rdx, rax}, {rdx, rax, rhs});
        emit(Opcode::MOVrd, {dest}, {rdx});
    }
}

static bool is32Bit(int64_t x)
{
    int32_t truncated = static_cast<int32_t>(x);
    int64_t signExtended = static_cast<int64_t>(truncated);;

    return x == signExtended;
}

void MachineCodeGen::visit(CallInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* target = getOperand(inst->function);
    assert(dest->isRegister());

    if (!inst->indirect)
    {
        assert(target->isAddress());
    }
    else
    {
        assert(target->isAddress() || target->isRegister());
    }

    size_t paramsOnStack = inst->params.size();

    // Keep 16-byte alignment
    if (paramsOnStack % 2)
    {
        emit(Opcode::PUSH, {}, {new Immediate(0)});
    }

    for (auto i = inst->params.rbegin(); i != inst->params.rend(); ++i)
    {
        MachineOperand* param = getOperand(*i);

        // No 64-bit immediate push
        if (param->isAddress() ||
            (param->isImmediate() && !is32Bit(dynamic_cast<Immediate*>(param)->value)))
        {
            VirtualRegister* vreg = new VirtualRegister(_nextVregNumber++);
            emit(Opcode::MOVrd, {vreg}, {param});
            emit(Opcode::PUSH, {}, {vreg});
        }
        else if (param->isRegister())
        {
            emit(Opcode::PUSH, {}, {param});
        }
        else if (param->isImmediate())
        {
            emit(Opcode::PUSH, {}, {param});
        }
        else
        {
            // We don't do direct push-from-memory
            assert(false);
        }
    }

    if (!inst->indirect)
    {
        emit(Opcode::CALLi, {rax}, {target});
    }
    else
    {
        emit(Opcode::CALLm, {rax}, {target});
    }

    emit(Opcode::MOVrd, {dest}, {rax});

    // Remove the function parameters from the stack
    if (paramsOnStack > 0)
    {
        emit(Opcode::ADD, {rsp}, {rsp, new Immediate(8 * paramsOnStack)});
    }
}

void MachineCodeGen::visit(ConditionalJumpInst* inst)
{
    MachineOperand* lhs = getOperand(inst->lhs);
    MachineOperand* rhs = getOperand(inst->rhs);
    assert(lhs->isRegister() || lhs->isImmediate());
    assert(rhs->isRegister() || rhs->isImmediate());

    // cmp imm, imm is illegal
    assert(!lhs->isImmediate() || !rhs->isImmediate());

    emit(Opcode::CMP, {}, {lhs, rhs});

    MachineOperand* ifTrue = getOperand(inst->ifTrue);
    MachineOperand* ifFalse = getOperand(inst->ifFalse);
    assert(ifTrue->isLabel() && ifFalse->isLabel());

    if (inst->op == ">")
    {
        emit(Opcode::JG, {}, {ifTrue});
    }
    else if (inst->op == "<")
    {
        emit(Opcode::JL, {}, {ifTrue});
    }
    else if (inst->op == "==")
    {
        emit(Opcode::JE, {}, {ifTrue});
    }
    else if (inst->op == "!=")
    {
        emit(Opcode::JNE, {}, {ifTrue});
    }
    else if (inst->op == ">=")
    {
        emit(Opcode::JGE, {}, {ifTrue});
    }
    else if (inst->op == "<=")
    {
        emit(Opcode::JLE, {}, {ifTrue});
    }
    else
    {
        assert(false);
    }

    emit(Opcode::JMP, {}, {ifFalse});
}

void MachineCodeGen::visit(CopyInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* src = getOperand(inst->src);
    assert(dest->isRegister());

    if (src->isRegister() || src->isImmediate())
    {
        emit(Opcode::MOVrd, {dest}, {src});
    }
    else
    {
        assert(false);
    }
}

void MachineCodeGen::visit(IndexedLoadInst* inst)
{
    MachineOperand* dest = getOperand(inst->lhs);
    MachineOperand* base = getOperand(inst->rhs);
    MachineOperand* offset = new Immediate(inst->offset);
    assert(dest->isRegister());
    assert(base->isAddress() || base->isRegister());

    emit(Opcode::MOVrm, {dest}, {base, offset});
}

void MachineCodeGen::visit(LoadInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* base = getOperand(inst->src);
    assert(dest->isRegister());
    assert(base->isAddress() || base->isRegister() || base->isStackLocation());

    emit(Opcode::MOVrm, {dest}, {base});
}

void MachineCodeGen::visit(IndexedStoreInst* inst)
{
    MachineOperand* base = getOperand(inst->lhs);
    MachineOperand* offset = new Immediate(inst->offset);
    MachineOperand* src = getOperand(inst->rhs);
    assert(base->isAddress() || base->isRegister());

    if (src->isRegister() || src->isImmediate() || src->isAddress())
    {
        emit(Opcode::MOVmd, {}, {base, src, offset});
    }
    else
    {
        assert(false);
    }
}

void MachineCodeGen::visit(StoreInst* inst)
{
    MachineOperand* base = getOperand(inst->dest);
    MachineOperand* src = getOperand(inst->src);
    assert(base->isAddress() || base->isRegister());

    if (src->isRegister() || src->isImmediate() || src->isAddress())
    {
        emit(Opcode::MOVmd, {}, {base, src});
    }
    else
    {
        assert(false);
    }
}

void MachineCodeGen::visit(JumpIfInst* inst)
{
    MachineOperand* condition = getOperand(inst->lhs);
    MachineOperand* ifTrue = getOperand(inst->ifTrue);
    MachineOperand* ifFalse = getOperand(inst->ifFalse);
    assert(condition->isRegister() || condition->isImmediate());
    assert(ifTrue->isLabel() && ifFalse->isLabel());

    // CMP imm, imm is illegal
    // TODO: Optimize this away instead of handling it here
    if (condition->isImmediate())
    {
        int64_t value = dynamic_cast<Immediate*>(condition)->value;
        if (value == 3)
        {
            emit(Opcode::JMP, {}, {ifTrue});
        }
        else
        {
            emit(Opcode::JMP, {}, {ifFalse});
        }
    }
    else
    {
        emit(Opcode::CMP, {}, {condition, new Immediate(3)});
        emit(Opcode::JE, {}, {ifTrue});
        emit(Opcode::JMP, {}, {ifFalse});
    }
}

void MachineCodeGen::visit(JumpInst* inst)
{
    MachineOperand* target = getOperand(inst->target);
    assert(target->isLabel());

    emit(Opcode::JMP, {}, {target});
}

void MachineCodeGen::visit(PhiInst* inst)
{
    assert(false);
}

void MachineCodeGen::visit(ReturnInst* inst)
{
    if (inst->value)
    {
        MachineOperand* value = getOperand(inst->value);
        if (value->isRegister() || value->isImmediate() || value->isAddress())
        {
            emit(Opcode::MOVrd, {rax}, {value});
        }
        else
        {
            assert(false);
        }
    }

    emit(Opcode::RET, {}, {rax});
}

void MachineCodeGen::visit(TagInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* src = getOperand(inst->src);
    assert(dest->isRegister());
    assert(src->isRegister() || src->isImmediate());

    emit(Opcode::MOVrd, {dest}, {src});
    emit(Opcode::SAL, {dest}, {dest, new Immediate(1)});
    emit(Opcode::INC, {dest}, {dest});
}

void MachineCodeGen::visit(UntagInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* src = getOperand(inst->src);
    assert(dest->isRegister());
    assert(src->isRegister() || src->isImmediate());

    emit(Opcode::MOVrd, {dest}, {src});
    emit(Opcode::SAR, {dest}, {dest, new Immediate(1)});
}

void MachineCodeGen::visit(UnreachableInst* inst)
{
}
