#include "tac_local_optimizer.hpp"

#define BOX(x) (((x) << 1) + 1)
#define UNBOX(x) ((x) >> 1)

#define REPLACE(x) (*_here).reset(x); (*_here)->accept(this);

#define CHECK_DEAD() if (_isDead) { _deleteHere = true; return; }

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
    for (_here = instructions.begin(); _here != instructions.end();)
    {
        _deleteHere = false;

        (*_here)->accept(this);

        auto prev = _here;
        ++_here;

        if (_deleteHere)
            instructions.erase(prev);
    }
}

void TACLocalOptimizer::replaceConstants(std::shared_ptr<Address>& operand)
{
    if (_constants.find(operand) != _constants.end())
    {
        operand = _constants[operand];
    }
}

void TACLocalOptimizer::visit(TACConditionalJump* inst)
{
    CHECK_DEAD();

    replaceConstants(inst->lhs);
    replaceConstants(inst->rhs);

    if (inst->lhs->isConst() && inst->rhs->isConst())
    {
        long lhs = UNBOX(inst->lhs->asConst().value);
        long rhs = UNBOX(inst->rhs->asConst().value);
        bool result;

        if (inst->op == ">")
        {
            result = (lhs > rhs);
        }
        else if (inst->op == "<")
        {
            result = (lhs < rhs);
        }
        else if (inst->op == "==")
        {
            result = (lhs == rhs);
        }
        else if (inst->op == "!=")
        {
            result = (lhs != rhs);
        }
        else if (inst->op == ">=")
        {
            result = (lhs >= rhs);
        }
        else if (inst->op == "<=")
        {
            result = (lhs <= rhs);
        }
        else
        {
            assert(false);
        }

        if (result)
        {
            REPLACE(new TACJump(inst->target));
        }
        else
        {
            _deleteHere = true;
        }
    }
}

void TACLocalOptimizer::visit(TACJumpIf* inst)
{
    CHECK_DEAD();

    replaceConstants(inst->lhs);
}

void TACLocalOptimizer::visit(TACJumpIfNot* inst)
{
    CHECK_DEAD();

    replaceConstants(inst->lhs);
}

void TACLocalOptimizer::visit(TACAssign* inst)
{
    CHECK_DEAD();

    if (inst->rhs->isConst())
    {
        _constants[inst->lhs] = inst->rhs;
    }
    else
    {
        _constants.erase(inst->lhs);
    }
}

void TACLocalOptimizer::visit(TACJump* inst)
{
    CHECK_DEAD();
    _isDead = true;
}

void TACLocalOptimizer::visit(TACLabel* inst)
{
    _isDead = false;
    clearConstants();
}

void TACLocalOptimizer::visit(TACCall* inst)
{
    CHECK_DEAD();
    if (inst->dest) _constants.erase(inst->dest);
}

void TACLocalOptimizer::visit(TACIndirectCall* inst)
{
    CHECK_DEAD();
    if (inst->dest) _constants.erase(inst->dest);
}

void TACLocalOptimizer::visit(TACRightIndexedAssignment* inst)
{
    CHECK_DEAD();
    if (inst->lhs) _constants.erase(inst->lhs);
}

void TACLocalOptimizer::visit(TACLeftIndexedAssignment* inst)
{
    CHECK_DEAD();
    replaceConstants(inst->rhs);
    if (inst->lhs) _constants.erase(inst->lhs);
}

void TACLocalOptimizer::visit(TACBinaryOperation* inst)
{
    CHECK_DEAD();
    replaceConstants(inst->lhs);
    replaceConstants(inst->rhs);

    // Constant expressions
    if (inst->lhs->isConst() && inst->rhs->isConst())
    {
        long lhs = inst->lhs->asConst().value;
        long rhs = inst->rhs->asConst().value;

        long result;
        if (inst->op == BinaryOperation::BADD)
        {
            result = BOX(UNBOX(lhs) + UNBOX(rhs));
        }
        else if (inst->op == BinaryOperation::BSUB)
        {
            result = BOX(UNBOX(lhs) - UNBOX(rhs));
        }
        else if (inst->op == BinaryOperation::BMUL)
        {
            result = BOX(UNBOX(lhs) * UNBOX(rhs));
        }
        else if (inst->op == BinaryOperation::BDIV)
        {
            assert(rhs != 0);
            result = BOX(UNBOX(lhs) / UNBOX(rhs));
        }
        else if (inst->op == BinaryOperation::BMOD)
        {
            assert(rhs != 0);
            result = BOX(UNBOX(lhs) % UNBOX(rhs));
        }
        else if (inst->op == BinaryOperation::UADD)
        {
            result = lhs + rhs;
        }
        else if (inst->op == BinaryOperation::UAND)
        {
            result = lhs & rhs;
        }
        else
        {
            assert(false);
        }

        REPLACE(new TACAssign(inst->dest, std::make_shared<ConstAddress>(result)));
    }
    else if (inst->op == BinaryOperation::BADD)
    {
        if (inst->lhs->isConst())
        {
            long value = inst->lhs->asConst().value;

            // 0 + x => x
            if (value == BOX(0))
            {
                REPLACE(new TACAssign(inst->dest, inst->rhs));
            }
            else
            {
                // Boxed a + x => Unboxed x + (a - 1)
                REPLACE(new TACBinaryOperation(
                    inst->dest,
                    inst->rhs,
                    BinaryOperation::UADD,
                    std::make_shared<ConstAddress>(value - 1)));
            }
        }
        else if (inst->rhs->isConst())
        {
            long value = inst->rhs->asConst().value;

            // x + 0 => x
            if (value == BOX(0))
            {
                REPLACE(new TACAssign(inst->dest, inst->lhs));
            }
            else
            {
                // Boxed a + x => Unboxed x + (a - 1)
                REPLACE(new TACBinaryOperation(
                    inst->dest,
                    inst->lhs,
                    BinaryOperation::UADD,
                    std::make_shared<ConstAddress>(value - 1)));
            }
        }
    }
    else if (inst->op == BinaryOperation::BSUB)
    {
        // x - 0 => x
        if (inst->rhs->isConst() && inst->rhs->asConst().value == BOX(0))
        {
            REPLACE(new TACAssign(inst->dest, inst->lhs));
        }
    }
    else if (inst->op == BinaryOperation::BMUL)
    {
        // x * 0 or 0 * x => 0
        if ((inst->lhs->isConst() && inst->lhs->asConst().value == BOX(0)) ||
            (inst->rhs->isConst() && inst->rhs->asConst().value == BOX(0)))
        {
            REPLACE(new TACAssign(inst->dest, std::make_shared<ConstAddress>(BOX(0))));
        }

        // x * 1 or 1 * x => x
        else if (inst->lhs->isConst() && inst->lhs->asConst().value == BOX(1))
        {
            REPLACE(new TACAssign(inst->dest, inst->rhs));
        }
        else if (inst->rhs->isConst() && inst->rhs->asConst().value == BOX(1))
        {
            REPLACE(new TACAssign(inst->dest, inst->lhs));
        }
    }
}

