#include "codegen/machine_codegen.hpp"
#include "ir/tac_instruction.hpp"
#include "ir/value.hpp"

MachineCodeGen::MachineCodeGen(MachineContext* context, Function* function)
{
    _context = context;
    _function = new MachineFunction(_context, function->name);
    _context->functions.push_back(_function);

    vrsp = _function->createPrecoloredReg(_context->rsp, ValueType::NonHeapAddress);
    vrbp = _function->createPrecoloredReg(_context->rbp, ValueType::NonHeapAddress);
    hrax = _context->rax;
    hrdx = _context->rdx;

    // Convert parameters from IR format to machine format
    for (size_t i = 0; i < function->params.size(); ++i)
    {
        Argument* arg = dynamic_cast<Argument*>(function->params[i]);
        ValueType argType = function->params[i]->type;

        _params[arg] = _function->createStackParameter(argType, arg->name, i);
    }

    bool entry = true;
    for (BasicBlock* irBlock : function->blocks)
    {
        _currentBlock = getBlock(irBlock);

        if (entry)
        {
            emit(Opcode::PUSHQ, {}, {vrbp});
            emitMovrd(vrbp, vrsp);
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
        return _context->createImmediate(constInt->value, constInt->type);
    }
    else if (GlobalValue* global = dynamic_cast<GlobalValue*>(value))
    {
        bool clinkage = global->tag == GlobalTag::ExternFunction;
        return _context->createGlobal(global->name, global->type, clinkage);
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

        ValueType type = value ? value->type : ValueType::U64;
        VirtualRegister* vreg = _function->createVreg(type);
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

    MachineBB* mbb = _function->createBlock(block->seqNumber);
    _blocks[block] = mbb;

    return mbb;
}

void MachineCodeGen::emitMovrd(MachineOperand* dest, MachineOperand* src)
{
    assert(dest->isRegister());

    if (src->isAddress())
    {
        assert(dest->size() == 64);

        // MOV reg, addr is illegal on mac
        emit(Opcode::LEA, {dest}, {src});
    }
    else if (src->isRegister())
    {
        if (dest->size() <= src->size())
        {
            emit(Opcode::MOVrd, {dest}, {src});
        }
        else
        {
            if (isSigned(src->type))
            {
                emit(Opcode::MOVSXrr, {dest}, {src});
            }
            else
            {
                emit(Opcode::MOVZXrr, {dest}, {src});
            }
        }
    }
    else if (src->isImmediate())
    {
        if (dest->size() < src->size())
        {
            uint64_t value = dynamic_cast<Immediate*>(src)->value;

            switch (dest->size())
            {
                case 32:
                    value = uint32_t(value);
                    break;

                case 16:
                    value = uint16_t(value);
                    break;

                case 8:
                    value = uint8_t(value);
                    break;

                default:
                    assert(false);
            }

            src = _context->createImmediate(value, src->type);
        }

        emit(Opcode::MOVrd, {dest}, {src});
    }
    else
    {
        assert(false);
    }
}

static bool is32Bit(int64_t x)
{
    int32_t truncated = static_cast<int32_t>(x);
    int64_t signExtended = static_cast<int64_t>(truncated);

    return x == signExtended;
}

void MachineCodeGen::emitMovmd(MachineOperand* base, MachineOperand* src, MachineOperand* offset)
{
    assert(base->isAddress() || base->isRegister());
    assert(src->isRegister() || src->isImmediate() || src->isAddress());

    // MOV [mem], imm64 is illegal on x86-64
    // MOV [mem], addr64 is illegal on Mac
    if (src->isAddress() ||
        (src->isImmediate() && !is32Bit(dynamic_cast<Immediate*>(src)->value)))
    {
        VirtualRegister* tmp = _function->createVreg(src->type);
        emitMovrd(tmp, src);
        src = tmp;
    }

    if (offset)
    {
        assert(offset->size() == 64);
        emit(Opcode::MOVmd, {}, {base, src, offset});
    }
    else
    {
        emit(Opcode::MOVmd, {}, {base, src});
    }
}

void MachineCodeGen::visit(BinaryOperationInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* lhs = getOperand(inst->lhs);
    MachineOperand* rhs = getOperand(inst->rhs);

    assert(dest->isRegister());
    assert(lhs->isRegister() || lhs->isImmediate());
    assert(rhs->isRegister() || rhs->isImmediate());
    assert(dest->size() == lhs->size() && lhs->size() == rhs->size());

    if (inst->op == BinaryOperation::ADD)
    {
        emitMovrd(dest, lhs);
        emit(Opcode::ADD, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::SUB)
    {
        emitMovrd(dest, lhs);
        emit(Opcode::SUB, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::MUL)
    {
        if (dest->size() == 8)
        {
            // IMUL reg8, reg8 instruction doesn't exist. Must use AL for dest & lhs
            VirtualRegister* al = _function->createPrecoloredReg(hrax, dest->type);
            emitMovrd(al, lhs);
            emit(Opcode::IMUL, {al}, {al, rhs});
            emitMovrd(dest, al);
        }
        else
        {
            emitMovrd(dest, lhs);
            emit(Opcode::IMUL, {dest}, {dest, rhs});
        }
    }
    else if (inst->op == BinaryOperation::AND)
    {
        assert(dest->size() == lhs->size() && lhs->size() == rhs->size());

        emitMovrd(dest, lhs);
        emit(Opcode::AND, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::SHL)
    {
        assert(dest->size() == lhs->size());
        assert(rhs->isImmediate());

        int64_t value = dynamic_cast<Immediate*>(rhs)->value;
        assert(value < 32);

        emitMovrd(dest, lhs);
        emit(Opcode::SAL, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::SHR)
    {
        // TODO: Add support for unsigned right shift
        assert(!isSigned(lhs->type));
        assert(dest->size() == lhs->size());

        assert(rhs->isImmediate());
        int64_t value = dynamic_cast<Immediate*>(rhs)->value;
        assert(value < 32);

        emitMovrd(dest, lhs);
        emit(Opcode::SAR, {dest}, {dest, rhs});
    }
    else if (inst->op == BinaryOperation::DIV || inst->op == BinaryOperation::MOD)
    {
        assert(lhs->type == rhs->type && rhs->type == dest->type && isInteger(dest->type));

        ValueType type = dest->type;

        // No DIV/IDIV imm instruction
        if (rhs->isImmediate())
        {
            VirtualRegister* vreg = _function->createVreg(type);
            emitMovrd(vreg, rhs);
            rhs = vreg;
        }

        if (dest->size() != 8)
        {
            VirtualRegister* vrax = _function->createPrecoloredReg(hrax, type);
            VirtualRegister* vrdx = _function->createPrecoloredReg(hrdx, type);

            emitMovrd(vrax, lhs);

            if (isSigned(type))
            {
                emit(Opcode::CQO, {vrdx}, {vrax});
                emit(Opcode::IDIV, {vrdx, vrax}, {vrdx, vrax, rhs});
            }
            else
            {
                MachineOperand* zero = _context->createImmediate(0, type);
                emitMovrd(vrdx, zero);
                emit(Opcode::DIV, {vrdx, vrax}, {vrdx, vrax, rhs});
            }

            if (inst->op == BinaryOperation::DIV)
            {
                emitMovrd(dest, vrax);
            }
            else
            {
                emitMovrd(dest, vrdx);
            }
        }
        else
        {
            VirtualRegister* ax = _function->createPrecoloredReg(hrax, ValueType::U16);
            VirtualRegister* al = _function->createPrecoloredReg(hrax, type);

            // This will zero-extend or sign-extend as appropriate
            emitMovrd(ax, lhs);

            if (inst->op == BinaryOperation::DIV)
            {
                if (isSigned(type))
                {
                    emit(Opcode::IDIV, {al}, {ax, rhs});
                }
                else
                {
                    emit(Opcode::DIV, {al}, {ax, rhs});
                }

                emitMovrd(dest, al);
            }
            else
            {
                if (isSigned(type))
                {
                    emit(Opcode::IDIV, {ax}, {ax, rhs});
                }
                else
                {
                    emit(Opcode::DIV, {ax}, {ax, rhs});
                }

                // The code generator doesn't support direct references to ah
                emit(Opcode::SAR, {al}, {ax, _context->createImmediate(8, ValueType::U8)});
                emitMovrd(dest, al);
            }
        }
    }
    else
    {
        assert(false);
    }
}

void MachineCodeGen::visit(CallInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* target = getOperand(inst->function);
    assert(dest->isRegister());

    VirtualRegister* vrax = _function->createPrecoloredReg(hrax, dest->type);

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

            ValueType type = param->type;

            VirtualRegister* arg = _function->createPrecoloredReg(registerArgs[i], type);
            emitMovrd(arg, param);
            uses.push_back(arg);
        }

        if (inst->ccall)
        {
            VirtualRegister* vrax2 = _function->createPrecoloredReg(hrax, ValueType::NonHeapAddress);

            // Indirect call so that we can switch to the C stack
            emitMovrd(vrax2, target);
            uses[0] = _context->createGlobal("ccall", ValueType::NonHeapAddress, true);
            emit(Opcode::CALL, {vrax}, std::move(uses));
        }
        else
        {
            uses[0] = target;
            emit(Opcode::CALL, {vrax}, std::move(uses));
        }

        emitMovrd(dest, vrax);
    }
    else // native call convention: all arguments on the stack
    {
        assert(!inst->ccall);
        assert(target->isAddress() || target->isRegister());

        size_t paramsOnStack = inst->params.size();

        // Keep 16-byte alignment
        if (paramsOnStack % 2)
        {
            emit(Opcode::PUSHQ, {}, {_context->createImmediate(0, ValueType::U64)});
            ++paramsOnStack;
        }

        for (auto i = inst->params.rbegin(); i != inst->params.rend(); ++i)
        {
            MachineOperand* param = getOperand(*i);

            // No 64-bit immediate push
            if (param->isAddress() ||
                (param->isImmediate() && param->size() == 64 && !is32Bit(dynamic_cast<Immediate*>(param)->value)))
            {
                VirtualRegister* vreg = _function->createVreg(param->type);
                emitMovrd(vreg, param);
                emit(Opcode::PUSHQ, {}, {vreg});
            }
            else if (param->isRegister())
            {
                emit(Opcode::PUSHQ, {}, {param});
            }
            else if (param->isImmediate())
            {
                emit(Opcode::PUSHQ, {}, {param});
            }
            else
            {
                // We don't do direct push-from-memory
                assert(false);
            }
        }

        emit(Opcode::CALL, {vrax}, {target});
        emitMovrd(dest, vrax);

        // Remove the function parameters from the stack
        if (paramsOnStack > 0)
        {
            emit(Opcode::ADD, {vrsp}, {vrsp, _context->createImmediate(8 * paramsOnStack, ValueType::U64)});
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
        VirtualRegister* newLhs = _function->createVreg(lhs->type);
        emitMovrd(newLhs, lhs);
        lhs = newLhs;
    }

    // Immediates are at most 32-bit
    if (lhs->isImmediate())
        std::swap(lhs, rhs);

    if (rhs->isImmediate() && rhs->size() == 64 && !is32Bit(dynamic_cast<Immediate*>(rhs)->value))
    {
        VirtualRegister* newRhs = _function->createVreg(rhs->type);
        emitMovrd(newRhs, rhs);
        rhs = newRhs;
    }

    emit(Opcode::CMP, {}, {lhs, rhs});

    MachineOperand* ifTrue = getOperand(inst->ifTrue);
    MachineOperand* ifFalse = getOperand(inst->ifFalse);
    assert(ifTrue->isLabel() && ifFalse->isLabel());

    bool sign = isSigned(inst->lhs->type);

    Opcode opcode;
    if (inst->op == ">")
    {
        if (sign)
        {
            opcode = Opcode::JG;
        }
        else
        {
            opcode = Opcode::JA;
        }
    }
    else if (inst->op == "<")
    {
        if (sign)
        {
            opcode = Opcode::JL;
        }
        else
        {
            opcode = Opcode::JB;
        }
    }
    else if (inst->op == "==")
    {
        opcode = Opcode::JE;
    }
    else if (inst->op == "!=")
    {
        opcode = Opcode::JNE;
    }
    else if (inst->op == ">=")
    {
        if (sign)
        {
            opcode = Opcode::JGE;
        }
        else
        {
            opcode = Opcode::JAE;
        }
    }
    else if (inst->op == "<=")
    {
        if (sign)
        {
            opcode = Opcode::JLE;
        }
        else
        {
            opcode = Opcode::JBE;
        }
    }
    else
    {
        assert(false);
    }

    emit(opcode, {}, {ifTrue});
    emit(Opcode::JMP, {}, {ifFalse});
}

void MachineCodeGen::visit(CopyInst* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* src = getOperand(inst->src);
    assert(dest->isRegister());
    assert(src->isRegister() || src->isImmediate() || src->isAddress());

    emitMovrd(dest, src);
}

void MachineCodeGen::visit(IndexedLoadInst* inst)
{
    MachineOperand* dest = getOperand(inst->lhs);
    MachineOperand* base = getOperand(inst->rhs);
    MachineOperand* offset = getOperand(inst->offset);

    assert(dest->isRegister());
    assert(base->isAddress() || base->isRegister());
    assert(offset->isImmediate() || offset->isRegister());

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
    MachineOperand* offset = getOperand(inst->offset);
    MachineOperand* src = getOperand(inst->rhs);

    emitMovmd(base, src, offset);
}

void MachineCodeGen::visit(StoreInst* inst)
{
    MachineOperand* base = getOperand(inst->dest);
    MachineOperand* src = getOperand(inst->src);

    emitMovmd(base, src);
}

void MachineCodeGen::visit(JumpIfInst* inst)
{
    MachineOperand* condition = getOperand(inst->lhs);
    MachineOperand* ifTrue = getOperand(inst->ifTrue);
    MachineOperand* ifFalse = getOperand(inst->ifFalse);
    assert(condition->isRegister() || condition->isImmediate());
    assert(ifTrue->isLabel() && ifFalse->isLabel());
    assert(condition->size() == 64);

    // CMP imm, imm is illegal
    // TODO: Optimize this away instead of handling it here
    if (condition->isImmediate())
    {
        int64_t value = dynamic_cast<Immediate*>(condition)->value;
        if (value == 1)
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
        emit(Opcode::CMP, {}, {condition, _context->createImmediate(1, ValueType::U64)});
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
        VirtualRegister* vrax = _function->createPrecoloredReg(hrax, inst->value->type);

        MachineOperand* value = getOperand(inst->value);
        assert(value->isRegister() || value->isImmediate() || value->isAddress());

        emitMovrd(vrax, value);
        emitMovrd(vrsp, vrbp);
        emit(Opcode::POP, {vrbp}, {});
        emit(Opcode::RET, {}, {vrax});
    }
    else
    {
        emitMovrd(vrsp, vrbp);
        emit(Opcode::POP, {vrbp}, {});
        emit(Opcode::RET, {}, {});
    }
}

void MachineCodeGen::visit(UnreachableInst* inst)
{
}

void MachineCodeGen::visit(MemsetFn* inst)
{
    MachineOperand* dest = getOperand(inst->dest);
    MachineOperand* offset = getOperand(inst->offset);
    MachineOperand* count = getOperand(inst->count);
    MachineOperand* value = getOperand(inst->value);

    // This may point into the interior of an object, so calling it a reference
    // might break the GC
    VirtualRegister* vrdi = _function->createPrecoloredReg(_context->rdi, ValueType::U64);
    emitMovrd(vrdi, dest);
    emit(Opcode::ADD, {vrdi}, {vrdi, offset});

    VirtualRegister* vrcx = _function->createPrecoloredReg(_context->rcx, ValueType::U64);
    emitMovrd(vrcx, count);

    VirtualRegister* vrax = _function->createPrecoloredReg(hrax, inst->value->type);
    emitMovrd(vrax, value);

    emit(Opcode::REP_STOS, {}, {vrdi, vrcx, vrax});
}
