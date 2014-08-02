#ifndef TAC_LOCAL_OPTIMIZER_HPP
#define TAC_LOCAL_OPTIMIZER_HPP

#include "address.hpp"
#include "tac_instruction.hpp"
#include "tac_program.hpp"
#include "tac_visitor.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

class TACLocalOptimizer : public TACVisitor
{
public:
    void optimizeCode(TACProgram& program);
    void optimizeCode(TACFunction& program);

    void visit(TACConditionalJump* inst);
    void visit(TACJumpIf* inst);
    void visit(TACJumpIfNot* inst);
    void visit(TACAssign* inst);
    void visit(TACJump* inst);
    void visit(TACLabel* inst);
    void visit(TACCall* inst);
    void visit(TACIndirectCall* inst);
    void visit(TACRightIndexedAssignment* inst);
    void visit(TACLeftIndexedAssignment* inst);
    void visit(TACBinaryOperation* inst);

private:
    void replace(TACInstruction* newInst);
    void deleteCurrent();

    // This points to the next pointer of the previous instruction. In other
    // worse, **_here is the instruction we're currently working on. This is
    // so that by setting *_here, we can replace or delete the current
    // instruction
    TACInstruction** _here;

    // Keep track of locals / temporaries which are guaranteed to have given
    // constant values at this point in the execution. Needs to be cleared at
    // every label. Used for constant propagation.
    std::unordered_map<std::shared_ptr<Address>, std::shared_ptr<Address>> _constants;
    void clearConstants() { _constants.clear(); }
    void replaceConstants(std::shared_ptr<Address>& operand);

    // Everything between an unconditional jump and the next label is dead
    bool _isDead = false;
};

#endif
