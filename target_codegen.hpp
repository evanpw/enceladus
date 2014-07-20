#ifndef TARGET_CODE_GEN_HPP
#define TARGET_CODE_GEN_HPP

struct TACConditionalJump;
struct TACJumpIf;
struct TACJumpIfNot;
struct TACAssign;
struct TACJump;
struct TACLabel;
struct TACCall;
struct TACIndirectCall;
struct TACRightIndexedAssignment;
struct TACLeftIndexedAssignment;
struct TACBinaryOperation;
struct TACProgram;

struct TargetCodeGen
{
    virtual ~TargetCodeGen() {}

    virtual void generateCode(const TACProgram& program) = 0;

    virtual void codeGen(const TACConditionalJump* inst) = 0;
    virtual void codeGen(const TACJumpIf* inst) = 0;
    virtual void codeGen(const TACJumpIfNot* inst) = 0;
    virtual void codeGen(const TACAssign* inst) = 0;
    virtual void codeGen(const TACJump* inst) = 0;
    virtual void codeGen(const TACLabel* inst) = 0;
    virtual void codeGen(const TACCall* inst) = 0;
    virtual void codeGen(const TACIndirectCall* inst) = 0;
    virtual void codeGen(const TACRightIndexedAssignment* inst) = 0;
    virtual void codeGen(const TACLeftIndexedAssignment* inst) = 0;
    virtual void codeGen(const TACBinaryOperation* inst) = 0;
};

#endif
