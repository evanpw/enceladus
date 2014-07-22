#include "tac_local_optimizer.hpp"

#define BOXED(x) (((x) << 1) + 1)
#define UNBOXED(x) ((x) >> 1)

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
        long lhs = UNBOXED(inst->lhs->asConst().value);
        long rhs = UNBOXED(inst->rhs->asConst().value);

        long result;
        if (inst->op == "+")
        {
            result = lhs + rhs;
        }
        else if (inst->op == "-")
        {
            result = lhs - rhs;
        }
        else if (inst->op == "*")
        {
            result = lhs * rhs;
        }
        else if (inst->op == "/")
        {
            assert(rhs != 0);
            result = lhs / rhs;
        }
        else if (inst->op == "%")
        {
            assert(rhs != 0);
            result = lhs % rhs;
        }

        (*_here).reset(new TACAssign(inst->dest, std::make_shared<ConstAddress>(BOXED(result))));
    }
    else if (inst->op == "+")
    {
        // x + 0 or 0 + x => x
        if (inst->lhs->isConst() && inst->lhs->asConst().value == BOXED(0))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->rhs));
        }
        else if (inst->rhs->isConst() && inst->rhs->asConst().value == BOXED(0))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->lhs));
        }
    }
    else if (inst->op == "*")
    {
        // x * 0 or 0 * x => 0
        if ((inst->lhs->isConst() && inst->lhs->asConst().value == BOXED(0)) ||
            (inst->rhs->isConst() && inst->rhs->asConst().value == BOXED(0)))
        {
            (*_here).reset(new TACAssign(inst->dest, std::make_shared<ConstAddress>(BOXED(0))));
        }

        // x * 1 or 1 * x => x
        else if (inst->lhs->isConst() && inst->lhs->asConst().value == BOXED(1))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->rhs));
        }
        else if (inst->rhs->isConst() && inst->rhs->asConst().value == BOXED(1))
        {
            (*_here).reset(new TACAssign(inst->dest, inst->lhs));
        }
    }
}

