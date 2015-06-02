#ifndef TAC_INSTRUCTION_HPP
#define TAC_INSTRUCTION_HPP

#include "address.hpp"
#include "scope.hpp"
#include "tac_visitor.hpp"

#include <string>

struct TACInstruction
{
    virtual ~TACInstruction() { delete next; }

    virtual void accept(TACVisitor* visitor) = 0;

    virtual std::string str() const = 0;

    // Instructions in a single function form an instrusive linked list
    TACInstruction* next = nullptr;
};

#define MAKE_VISITABLE() virtual void accept(TACVisitor* visitor) { visitor->visit(this); }

struct TACComment : public TACInstruction
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

struct TACLabel : public TACInstruction
{
    TACLabel();

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << ".L" << number;
        return ss.str();
    }

    long number;
    static long labelCount;
};

struct TACConditionalJump : public TACInstruction
{
    TACConditionalJump(std::shared_ptr<Address> lhs, const std::string& op, std::shared_ptr<Address> rhs, TACLabel* target)
    : lhs(lhs), op(op), rhs(rhs), target(target)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "if " << lhs->str() << " " << op << " " << rhs->str() << " goto " << target->str();
        return ss.str();
    }

    std::shared_ptr<Address> lhs;
    std::string op;
    std::shared_ptr<Address> rhs;
    TACLabel* target;
};

struct TACJumpIf : public TACInstruction
{
    TACJumpIf(std::shared_ptr<Address> lhs, TACLabel* target)
    : lhs(lhs), target(target)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "if " << lhs->str() << " goto " << target->str();
        return ss.str();
    }

    std::shared_ptr<Address> lhs;
    TACLabel* target;
};

struct TACJumpIfNot : public TACInstruction
{
    TACJumpIfNot(std::shared_ptr<Address> lhs, TACLabel* target)
    : lhs(lhs), target(target)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "ifnot " << lhs->str() << " goto " << target->str();
        return ss.str();
    }

    std::shared_ptr<Address> lhs;
    TACLabel* target;
};

struct TACAssign : public TACInstruction
{
    TACAssign(std::shared_ptr<Address> lhs, std::shared_ptr<Address> rhs)
    : lhs(lhs), rhs(rhs)
    {
        assert(lhs->tag != AddressTag::Const);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << lhs->str() << " = " << rhs->str();
        return ss.str();
    }

    std::shared_ptr<Address> lhs;
    std::shared_ptr<Address> rhs;
};

struct TACJump : public TACInstruction
{
    TACJump(TACLabel* target)
    : target(target)
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "jump " << target->str();
        return ss.str();
    }

    TACLabel* target;
};

struct TACCall : public TACInstruction
{
    TACCall(bool foreign, std::shared_ptr<Address> dest, const std::string& function, const std::vector<std::shared_ptr<Address>>& params = {}, bool ccall = false)
    : foreign(foreign), dest(dest), function(function), params(params), ccall(ccall)
    {
    }

    TACCall(bool foreign, std::shared_ptr<Address> dest, const std::string& function, std::initializer_list<std::shared_ptr<Address>> paramsList, bool ccall = false)
    : foreign(foreign), dest(dest), function(function), ccall(ccall)
    {
        for (auto& param : paramsList) params.push_back(param);
    }

    TACCall(bool foreign, const std::string& function, std::initializer_list<std::shared_ptr<Address>> paramsList = {}, bool ccall = false)
    : foreign(foreign), function(function), ccall(ccall)
    {
        for (auto& param : paramsList) params.push_back(param);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;

        if (dest)
            ss << dest->str() << " = call " << function << "(";
        else
            ss << "call " << function << "(";

        for (size_t i = 0; i < params.size(); ++i)
        {
            ss << params[i]->str();
            if (i + 1 != params.size()) ss << ", ";
        }
        ss << ")";

        return ss.str();
    }

    bool foreign;
    std::shared_ptr<Address> dest;
    std::string function;
    std::vector<std::shared_ptr<Address>> params;
    bool ccall;
};

struct TACIndirectCall : public TACInstruction
{
    TACIndirectCall(std::shared_ptr<Address> dest, std::shared_ptr<Address> function, const std::vector<std::shared_ptr<Address>>& params)
    : dest(dest), function(function), params(params)
    {
        assert(function->tag == AddressTag::Temp);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << dest->str() << " = call " << function << "(";

        for (size_t i = 0; i < params.size(); ++i)
        {
            ss << params[i]->str();
            if (i + 1 != params.size()) ss << ", ";
        }
        ss << ")";

        return ss.str();
    }

    std::shared_ptr<Address> dest;
    std::shared_ptr<Address> function;
    std::vector<std::shared_ptr<Address>> params;
};

struct TACRightIndexedAssignment : public TACInstruction
{
    TACRightIndexedAssignment(std::shared_ptr<Address> lhs, std::shared_ptr<Address> rhs, size_t offset)
    : lhs(lhs), rhs(rhs), offset(offset)
    {
        assert(lhs->tag != AddressTag::Const && rhs->tag != AddressTag::Const);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << lhs->str() << " = " << rhs->str() << "[" << offset << "]";
        return ss.str();
    }

    std::shared_ptr<Address> lhs;
    std::shared_ptr<Address> rhs;
    size_t offset;
};

struct TACLeftIndexedAssignment : public TACInstruction
{
    TACLeftIndexedAssignment(std::shared_ptr<Address> lhs, size_t offset, std::shared_ptr<Address> rhs)
    : lhs(lhs), offset(offset), rhs(rhs)
    {
        assert(lhs->tag != AddressTag::Const);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << lhs->str() << "[" << offset << "] = " << rhs->str();
        return ss.str();
    }

    std::shared_ptr<Address> lhs;
    size_t offset;
    std::shared_ptr<Address> rhs;
};

enum class BinaryOperation {BADD, BSUB, BMUL, BDIV, BMOD, UAND, UADD};
extern const char* binaryOperationNames[];

struct TACBinaryOperation : public TACInstruction
{
    TACBinaryOperation(std::shared_ptr<Address> dest, std::shared_ptr<Address> lhs, BinaryOperation op, std::shared_ptr<Address> rhs)
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

    std::shared_ptr<Address> dest;
    std::shared_ptr<Address> lhs;
    BinaryOperation op;
    std::shared_ptr<Address> rhs;
};

#endif
