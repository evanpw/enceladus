#include "constant_folding.hpp"
#include "tac_instruction.hpp"

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

static bool getConstant(Value* value, int64_t& out)
{
    if (ConstantInt* constInt = dynamic_cast<ConstantInt*>(value))
    {
        out = constInt->value;
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
    int64_t lhs, rhs;
    if (!getConstant(inst->lhs, lhs) || !getConstant(inst->rhs, rhs))
        return;

    int64_t resultValue;

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
        assert(rhs != 0);
        resultValue = lhs / rhs;
    }
    else if (inst->op == BinaryOperation::MOD)
    {
        assert(rhs != 0);
        resultValue = lhs % rhs;
    }
    else
    {
        assert(false);
    }

    Value* result = _context->getConstantInt(resultValue);
    inst->removeFromParent();
    _function->replaceReferences(inst->dest, result);
}

void ConstantFolding::visit(TagInst* inst)
{
    int64_t value;
    if (getConstant(inst->src, value))
    {
        Value* taggedInt = _context->getConstantInt((value << 1) + 1);
        inst->removeFromParent();
        _function->replaceReferences(inst->dest, taggedInt);
    }
}

void ConstantFolding::visit(UntagInst* inst)
{
    int64_t value;
    if (getConstant(inst->src, value))
    {
        Value* untaggedInt = _context->getConstantInt(value >> 1);
        inst->removeFromParent();
        _function->replaceReferences(inst->dest, untaggedInt);
    }
}
