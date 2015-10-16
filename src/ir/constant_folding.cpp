#include "ir/constant_folding.hpp"
#include "ir/tac_instruction.hpp"

ConstantFolding::ConstantFolding(Function* function)
: _function(function), _context(function->context())
{
}

void ConstantFolding::run()
{
    for (BasicBlock* block : _function->blocks)
    {
        Instruction* inst = block->first;
        while (inst != nullptr)
        {
            Instruction* next = inst->next;
            inst->accept(this);
            inst = next;
        }
    }
}

static bool getConstant(Value* value, uint64_t& out, ValueType& type)
{
    if (ConstantInt* constInt = dynamic_cast<ConstantInt*>(value))
    {
        out = constInt->value;
        type = constInt->type;
        return true;
    }
    else
    {
        return false;
    }
}

void ConstantFolding::visit(CopyInst* inst)
{
}

void ConstantFolding::visit(BinaryOperationInst* inst)
{
    uint64_t lhs, rhs;
    ValueType lhsType, rhsType;

    if (!getConstant(inst->lhs, lhs, lhsType) || !getConstant(inst->rhs, rhs, rhsType))
        return;

    assert(lhsType == rhsType && isInteger(lhsType));
    ValueType type = lhsType;

    uint64_t resultValue;

    if (inst->op == BinaryOperation::ADD)
    {
        resultValue = lhs + rhs;
    }
    else if (inst->op == BinaryOperation::SUB)
    {
        resultValue = lhs - rhs;
    }
    else if (inst->op == BinaryOperation::MUL)
    {
        resultValue = lhs * rhs;
    }
    else if (inst->op == BinaryOperation::AND)
    {
        resultValue = lhs & rhs;
    }
    else if (inst->op == BinaryOperation::SHL)
    {
        assert(rhs < 32);
        resultValue = lhs << rhs;
    }
    else if (inst->op == BinaryOperation::SHR)
    {
        assert(rhs < 32);
        resultValue = lhs >> rhs;
    }
    else if (inst->op == BinaryOperation::DIV)
    {
        // TODO: Throw exception and explain
        assert(rhs != 0);

        if (isSigned(type))
        {
            resultValue = int64_t(lhs) / int64_t(rhs);
        }
        else
        {
            resultValue = lhs / rhs;
        }
    }
    else if (inst->op == BinaryOperation::MOD)
    {
        // TODO: Throw exception and explain
        assert(rhs != 0);

        if (isSigned(type))
        {
            resultValue = int64_t(lhs) % int64_t(rhs);
        }
        else
        {
            resultValue = lhs % rhs;
        }
    }
    else
    {
        assert(false);
    }

    // Narrow the result if not 64-bit
    switch (getSize(type))
    {
        case 64:
            break;

        case 32:
            resultValue = uint32_t(resultValue);
            break;

        case 16:
            resultValue = uint16_t(resultValue);
            break;

        case 8:
            resultValue = uint8_t(resultValue);
            break;

        default:
            assert(false);
    }

    Value* result = _context->createConstantInt(type, resultValue);
    inst->removeFromParent();
    _function->replaceReferences(inst->dest, result);
}
