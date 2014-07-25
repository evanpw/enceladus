#include "tac_local_optimizer.hpp"

#define BOX(x) (((x) << 1) + 1)
#define UNBOX(x) ((x) >> 1)

void TACLocalOptimizer::optimizeCode(TACProgram& program)
{
    // Main function
    optimizeCode(program.mainFunction);

    // All other functions
    for (auto& function : program.otherFunctions)
    {
        optimizeCode(function);
    }
}

void TACLocalOptimizer::optimizeCode(TACFunction& function)
{
    auto& instructions = function.instructions;
    for (_here = instructions.begin(); _here != instructions.end(); ++_here)
    {
        (*_here)->accept(this);
    }
}

void TACLocalOptimizer::visit(const TACConditionalJump* inst)
{
}

void TACLocalOptimizer::visit(const TACJumpIf* inst)
{
}

void TACLocalOptimizer::visit(const TACJumpIfNot* inst)
{
}

void TACLocalOptimizer::visit(const TACAssign* inst)
{
}

void TACLocalOptimizer::visit(const TACJump* inst)
{
}

void TACLocalOptimizer::visit(const TACLabel* inst)
{
}

void TACLocalOptimizer::visit(const TACCall* inst)
{
}

void TACLocalOptimizer::visit(const TACIndirectCall* inst)
{
}

void TACLocalOptimizer::visit(const TACRightIndexedAssignment* inst)
{
}

void TACLocalOptimizer::visit(const TACLeftIndexedAssignment* inst)
{
}

void TACLocalOptimizer::visit(const TACBinaryOperation* inst)
{
    // Constant expressions
    if (inst->lhs->isConst() && inst->rhs->isConst())
    {
        long lhs = UNBOX(inst->lhs->asConst().value);
        long rhs = UNBOX(inst->rhs->asConst().value);

        long result;
        if (inst->op == BinaryOperation::BADD)
        {
            result = lhs + rhs;
        }
        else if (inst->op == BinaryOperation::BSUB)
        {
            result = lhs - rhs;
        }
        else if (inst->op == BinaryOperation::BMUL)
        {
            result = lhs * rhs;
        }
        else if (inst->op == BinaryOperation::BDIV)
        {
            assert(rhs != 0);
            result = lhs / rhs;
        }
        else if (inst->op == BinaryOperation::BMOD)
        {
            assert(rhs != 0);
            result = lhs % rhs;
        }

        (*_here).reset(new TACAssign(inst->dest, std::make_shared<ConstAddress>(BOX(result))));
    }
    else if (inst->op == BinaryOperation::BADD)
    {
        // x + 0 or 0 + x => x
        if (inst->lhs->isConst() && inst->lhs->asConst().value == BOX(0))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->rhs));
        }
        else if (inst->rhs->isConst() && inst->rhs->asConst().value == BOX(0))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->lhs));
        }
    }
    else if (inst->op == BinaryOperation::BSUB)
    {
        // x - 0 => x
        if (inst->rhs->isConst() && inst->rhs->asConst().value == BOX(0))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->rhs));
        }
    }
    else if (inst->op == BinaryOperation::BMUL)
    {
        // x * 0 or 0 * x => 0
        if ((inst->lhs->isConst() && inst->lhs->asConst().value == BOX(0)) ||
            (inst->rhs->isConst() && inst->rhs->asConst().value == BOX(0)))
        {
            (*_here).reset(new TACAssign(inst->dest, std::make_shared<ConstAddress>(BOX(0))));
        }

        // x * 1 or 1 * x => x
        else if (inst->lhs->isConst() && inst->lhs->asConst().value == BOX(1))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->rhs));
        }
        else if (inst->rhs->isConst() && inst->rhs->asConst().value == BOX(1))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->lhs));
        }
    }
}

