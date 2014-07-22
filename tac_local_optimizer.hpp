#ifndef TAC_LOCAL_OPTIMIZER_HPP
#define TAC_LOCAL_OPTIMIZER_HPP

#include "address.hpp"
#include "tac_instruction.hpp"
#include "tac_program.hpp"
#include "tac_visitor.hpp"

#include <memory>
#include <vector>

class TACLocalOptimizer : public TACVisitor
{
public:
    void optimizeCode(TACProgram& program);
    void optimizeCode(TACFunction& program);

    void visit(const TACConditionalJump* inst);
    void visit(const TACJumpIf* inst);
    void visit(const TACJumpIfNot* inst);
    void visit(const TACAssign* inst);
    void visit(const TACJump* inst);
    void visit(const TACLabel* inst);
    void visit(const TACCall* inst);
    void visit(const TACIndirectCall* inst);
    void visit(const TACRightIndexedAssignment* inst);
    void visit(const TACLeftIndexedAssignment* inst);
    void visit(const TACBinaryOperation* inst);

private:
    // Points to the current instruction, so that we can replace it;
    std::vector<std::unique_ptr<TACInstruction>>::iterator _here;
};

#endif
