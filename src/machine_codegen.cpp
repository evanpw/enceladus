#include "machine_codegen.hpp"
#include "tac_instruction.hpp"
#include "value.hpp"

static OperandType getOperandType(Value* value)
{
    if (!value || value->type != ValueType::BoxOrInt)
    {
        return NotReference;
    }
    else
    {
        return MaybeReference;
    }
}

MachineCodeGen::MachineCodeGen(MachineContext* context, Function* function)
{
    _context = context;
    _function = new MachineFunction(_context, function->name);
    _context->functions.push_back(_function);

    vrsp = _function->makePrecoloredReg(_context->rsp, NotReference);
    vrbp = _function->makePrecoloredReg(_context->rbp, NotReference);
    hrax = _context->rax;
    hrdx = _context->rdx;

    // Convert parameters from IR format to machine format
    for (size_t i = 0; i < function->params.size(); ++i)
    {
        Argument* arg = dynamic_cast<Argument*>(function->params[i]);
        OperandType argType = getOperandType(function->params[i]);

        _params[arg] = _function->makeStackParameter(argType, arg->name, i);
    }

    bool entry = true;
    for (BasicBlock* irBlock : function->blocks)
    {
        _currentBlock = getBlock(irBlock);

        if (entry)
        {
            emit(Opcode::PUSH, {}, {vrbp});
            emit(Opcode::MOVrd, {vrbp}, {vrsp});
            entry = false;
        }

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
        return _context->makeImmediate(constInt->value);
    }
    else if (GlobalValue* global = dynamic_cast<GlobalValue*>(value))
    {
        return _context->makeGlobal(global->name);
    }
    else if (dynamic_cast<LocalValue*>(value))
    {
        // These should have all been converted to temporaries
        assert(false);
    }
    else if (Argument* arg = dynamic_cast<Argument*>(value))
    {
        return _params.at(arg);
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

        VirtualRegister* vreg = _function->makeVreg(getOperandType(value));
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

    MachineBB* mbb = _function->makeBlock(block->seqNumber);
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
        // No IDIV imm instruction
        if (rhs->isImmediate())
        {
            VirtualRegister* vreg = _function->makeVreg(NotReference);
            emit(Opcode::MOVrd, {vreg}, {rhs});
            rhs = vreg;
        }

        VirtualRegister* vrax = _function->makePrecoloredReg(hrax, NotReference);
        VirtualRegister* vrdx = _function->makePrecoloredReg(hrdx, NotReference);

        emit(Opcode::MOVrd, {vrax}, {lhs});
        emit(Opcode::CQO, {vrdx}, {vrax});
        emit(Opcode::IDIV, {vrdx, vrax}, {vrdx, vrax, rhs});
        emit(Opcode::MOVrd, {dest}, {vrax});
    }
    else if (inst->op == BinaryOperation::MOD)
    {
        // No IDIV imm instruction
        if (rhs->isImmediate())
        {
            VirtualRegister* vreg = _function->makeVreg(NotReference);
            emit(Opcode::MOVrd, {vreg}, {rhs});
            rhs = vreg;
        }

        VirtualRegister* vrax = _function->makePrecoloredReg(hrax, NotReference);
        VirtualRegister* vrdx = _function->makePrecoloredReg(hrdx, NotReference);

        emit(Opcode::MOVrd, {vrax}, {lhs});
        emit(Opcode::CQO, {vrdx}, {vrax});
        emit(Opcode::IDIV, {vrdx, vrax}, {vrdx, vrax, rhs});
        emit(Opcode::MOVrd, {dest}, {vrdx});
    }
    else
    {
        assert(false);
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
    OperandType destType = getOperandType(inst->dest);
    MachineOperand* target = getOperand(inst->function);
    assert(dest->isRegister());

    VirtualRegister* vrax = _function->makePrecoloredReg(hrax, destType);

    // ccall: pass arguments in registers, indirectly through ccall
    if (inst->regpass)
    {
        assert(target->isAddress());

        // x86_64 calling convention for C puts the first 6 arguments in registers
        HardwareRegister* registerArgs[] = {
            _context->rdi,
            _context->rsi,
            _context->rdx,
            _context->rcx,
            _context->r8,
            _context->r9
        };

        assert(inst->params.size() <= 6);

        std::vector<MachineOperand*> uses = {nullptr};
        for (size_t i = 0; i < inst->params.size(); ++i)
        {
            MachineOperand* param = getOperand(inst->params[i]);
            assert(param->isAddress() || param->isImmediate() || param->isRegister());

            VirtualRegister* arg = _function->makePrecoloredReg(registerArgs[i], getOperandType(inst->params[i]));
            emit(Opcode::MOVrd, {arg}, {param});
            uses.push_back(arg);
        }

        if (inst->ccall)
        {
            VirtualRegister* vrax2 = _function->makePrecoloredReg(hrax, NotReference);

            // Indirect call so that we can switch to the C stack
            emit(Opcode::MOVrd, {vrax2}, {target});
            uses[0] = _context->makeGlobal("ccall");
            emit(Opcode::CALL, {vrax}, std::move(uses));
        }
        else
        {
            uses[0] = target;
            emit(Opcode::CALL, {vrax}, std::move(uses));
        }

        emit(Opcode::MOVrd, {dest}, {vrax});
    }
    else // native call convention: all arguments on the stack
    {
        assert(!inst->ccall);
        assert(target->isAddress() || target->isRegister());

        size_t paramsOnStack = inst->params.size();

        // Keep 16-byte alignment
        if (paramsOnStack % 2)
        {
            emit(Opcode::PUSH, {}, {_context->makeImmediate(0)});
            ++paramsOnStack;
        }

        for (auto i = inst->params.rbegin(); i != inst->params.rend(); ++i)
        {
            MachineOperand* param = getOperand(*i);

            // No 64-bit immediate push
            if (param->isAddress() ||
                (param->isImmediate() && !is32Bit(dynamic_cast<Immediate*>(param)->value)))
            {
                VirtualRegister* vreg = _function->makeVreg(getOperandType(*i));
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

        emit(Opcode::CALL, {vrax}, {target});
        emit(Opcode::MOVrd, {dest}, {vrax});

        // Remove the function parameters from the stack
        if (paramsOnStack > 0)
        {
            emit(Opcode::ADD, {vrsp}, {vrsp, _context->makeImmediate(8 * paramsOnStack)});
        }
    }
}

void MachineCodeGen::visit(ConditionalJumpInst* inst)
{
    MachineOperand* lhs = getOperand(inst->lhs);
    MachineOperand* rhs = getOperand(inst->rhs);
    assert(lhs->isRegister() || lhs->isImmediate());
    assert(rhs->isRegister() || rhs->isImmediate());

    // cmp imm, imm is illegal (this should really be optimized away)
    if (lhs->isImmediate() && rhs->isImmediate())
    {
        VirtualRegister* newLhs = _function->makeVreg(NotReference);
        emit(Opcode::MOVrd, {newLhs}, {lhs});
        lhs = newLhs;
    }

    // Immediates are 32-bit
    if (lhs->isImmediate())
        std::swap(lhs, rhs);

    if (rhs->isImmediate() && !is32Bit(dynamic_cast<Immediate*>(rhs)->value))
    {
        VirtualRegister* newRhs = _function->makeVreg(NotReference);
        emit(Opcode::MOVrd, {newRhs}, {rhs});
        rhs = newRhs;
    }

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
    MachineOperand* offset = _context->makeImmediate(inst->offset);

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
    MachineOperand* offset = _context->makeImmediate(inst->offset);
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
        // MOV [mem], imm64 is illegal
        if (src->isImmediate() && !is32Bit(dynamic_cast<Immediate*>(src)->value))
        {
            VirtualRegister* vreg = _function->makeVreg(NotReference);
            emit(Opcode::MOVrd, {vreg}, {src});
            emit(Opcode::MOVmd, {}, {base, vreg});
        }
        else
        {
            emit(Opcode::MOVmd, {}, {base, src});
        }
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
        emit(Opcode::CMP, {}, {condition, _context->makeImmediate(3)});
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
        OperandType destType = getOperandType(inst->value);
        VirtualRegister* vrax = _function->makePrecoloredReg(hrax, destType);

        MachineOperand* value = getOperand(inst->value);
        assert(value->isRegister() || value->isImmediate() || value->isAddress());

        emit(Opcode::MOVrd, {vrax}, {value});
        emit(Opcode::MOVrd, {vrsp}, {vrbp});
        emit(Opcode::POP, {vrbp}, {});
        emit(Opcode::RET, {}, {vrax});
    }
    else
    {
        emit(Opcode::MOVrd, {vrsp}, {vrbp});
        emit(Opcode::POP, {vrbp}, {});
        emit(Opcode::RET, {}, {});
    }
}

void MachineCodeGen::visit(TagInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* src = getOperand(inst->src);
    assert(dest->isRegister());
    assert(src->isRegister() || src->isImmediate());

    emit(Opcode::MOVrd, {dest}, {src});
    emit(Opcode::SAL, {dest}, {dest, _context->makeImmediate(1)});
    emit(Opcode::INC, {dest}, {dest});
}

void MachineCodeGen::visit(UntagInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* src = getOperand(inst->src);
    assert(dest->isRegister());
    assert(src->isRegister() || src->isImmediate());

    emit(Opcode::MOVrd, {dest}, {src});
    emit(Opcode::SAR, {dest}, {dest, _context->makeImmediate(1)});
}

void MachineCodeGen::visit(UnreachableInst* inst)
{
}
