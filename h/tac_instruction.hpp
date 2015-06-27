#ifndef TAC_INSTRUCTION_HPP
#define TAC_INSTRUCTION_HPP

#include "basic_block.hpp"
#include "scope.hpp"
#include "tac_visitor.hpp"
#include "value.hpp"

#include <string>

struct Instruction
{
    virtual ~Instruction() {}

    virtual void accept(TACVisitor* visitor) = 0;

    virtual std::string str() const = 0;

    void insertAfter(Instruction* inst)
    {
        assert(inst);
        Instruction* myNext = inst->next;

        inst->next = this;
        this->prev = inst;

        this->next = myNext;
        if (myNext) myNext->prev = this;
    }

    BasicBlock* parent;

    // Instructions in a single basic block form an instrusive linked list
    Instruction* prev = nullptr;
    Instruction* next = nullptr;
};

#define MAKE_VISITABLE() virtual void accept(TACVisitor* visitor) { visitor->visit(this); }

struct TACComment : public Instruction
{
    TACComment(const std::string& text)
    : text(text)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        return "comment " + text;
    }

    std::string text;
};

struct TACConditionalJump : public Instruction
{
    TACConditionalJump(Value* lhs, const std::string& op, Value* rhs, BasicBlock* ifTrue, BasicBlock* ifFalse)
    : lhs(lhs), op(op), rhs(rhs), ifTrue(ifTrue), ifFalse(ifFalse)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "br " << lhs->str() << " " << op << " " << rhs->str() << ", " << ifTrue->str() << ", " << ifFalse->str();
        return ss.str();
    }

    Value* lhs;
    std::string op;
    Value* rhs;
    BasicBlock* ifTrue;
    BasicBlock* ifFalse;
};

struct TACJumpIf : public Instruction
{
    TACJumpIf(Value* lhs, BasicBlock* ifTrue, BasicBlock* ifFalse)
    : lhs(lhs), ifTrue(ifTrue), ifFalse(ifFalse)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "br " << lhs->str() << ", " << ifTrue->str() << ", " << ifFalse->str();
        return ss.str();
    }

    Value* lhs;
    BasicBlock* ifTrue;
    BasicBlock* ifFalse;
};

struct TACAssign : public Instruction
{
    TACAssign(Value* lhs, Value* rhs)
    : lhs(lhs), rhs(rhs)
    {
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << lhs->str() << " = " << rhs->str();
        return ss.str();
    }

    Value* lhs;
    Value* rhs;
};

struct TACReturn : public Instruction
{
    TACReturn(Value* value = nullptr)
    : value(value)
    {
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        if (value)
        {
            std::stringstream ss;
            ss << "return " << value->str();
            return ss.str();
        }
        else
        {
            return "return";
        }
    }

    Value* value;
};

struct TACJump : public Instruction
{
    TACJump(BasicBlock* target)
    : target(target)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "jump " << target->str();
        return ss.str();
    }

    BasicBlock* target;
};

struct TACCall : public Instruction
{
    TACCall(bool foreign, Value* dest, Value* function, const std::vector<Value*>& params = {}, bool ccall = false)
    : foreign(foreign), dest(dest), function(function), params(params), ccall(ccall)
    {
    }

    TACCall(bool foreign, Value* dest, Value* function, std::initializer_list<Value*> paramsList, bool ccall = false)
    : foreign(foreign), dest(dest), function(function), ccall(ccall)
    {
        for (auto& param : paramsList) params.push_back(param);
    }

    TACCall(bool foreign, Value* function, std::initializer_list<Value*> paramsList = {}, bool ccall = false)
    : foreign(foreign), function(function), ccall(ccall)
    {
        for (auto& param : paramsList) params.push_back(param);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;

        if (dest)
            ss << dest->str() << " = call " << function->str() << "(";
        else
            ss << "call " << function->str() << "(";

        for (size_t i = 0; i < params.size(); ++i)
        {
            ss << params[i]->str();
            if (i + 1 != params.size()) ss << ", ";
        }
        ss << ")";

        return ss.str();
    }

    bool foreign;
    Value* dest;
    Value* function;
    std::vector<Value*> params;
    bool ccall;
};

struct TACIndirectCall : public Instruction
{
    TACIndirectCall(Value* dest, Value* function, const std::vector<Value*>& params)
    : dest(dest), function(function), params(params)
    {
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << dest->str() << " = call " << function->str() << "(";

        for (size_t i = 0; i < params.size(); ++i)
        {
            ss << params[i]->str();
            if (i + 1 != params.size()) ss << ", ";
        }
        ss << ")";

        return ss.str();
    }

    Value* dest;
    Value* function;
    std::vector<Value*> params;
};

struct TACRightIndexedAssignment : public Instruction
{
    TACRightIndexedAssignment(Value* lhs, Value* rhs, int64_t offset,
                              Value* index = nullptr, int64_t scale = 1)
    : lhs(lhs), rhs(rhs), offset(offset), index(index), scale(scale)
    {
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << lhs->str() << " = " << "[" << rhs->str() << " + " << offset << "]";
        return ss.str();
    }

    Value* lhs;
    Value* rhs;
    int64_t offset;
    Value* index;
    int64_t scale;
};

struct TACLeftIndexedAssignment : public Instruction
{
    TACLeftIndexedAssignment(Value* lhs, size_t offset, Value* rhs)
    : lhs(lhs), offset(offset), rhs(rhs)
    {
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "[" << lhs->str() << " + " << offset << "] = " << rhs->str();
        return ss.str();
    }

    Value* lhs;
    size_t offset;
    Value* rhs;
};

enum class BinaryOperation {TADD, TSUB, TMUL, TDIV, TMOD, UAND, UADD, SHR, SHL};
extern const char* binaryOperationNames[];

struct TACBinaryOperation : public Instruction
{
    TACBinaryOperation(Value* dest, Value* lhs, BinaryOperation op, Value* rhs)
    : dest(dest), lhs(lhs), op(op), rhs(rhs)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << dest->str()
           << " = "
           << lhs->str()
           << " "
           << binaryOperationNames[int(op)]
           << " "
           << rhs->str();
        return ss.str();
    }

    Value* dest;
    Value* lhs;
    BinaryOperation op;
    Value* rhs;
};

#endif
