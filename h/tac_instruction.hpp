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

struct ConditionalJumpInst : public Instruction
{
    ConditionalJumpInst(Value* lhs, const std::string& op, Value* rhs, BasicBlock* ifTrue, BasicBlock* ifFalse)
    : lhs(lhs), op(op), rhs(rhs), ifTrue(ifTrue), ifFalse(ifFalse)
    {
        lhs->uses.push_back(this);
        rhs->uses.push_back(this);
    }

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

struct JumpIfInst : public Instruction
{
    JumpIfInst(Value* lhs, BasicBlock* ifTrue, BasicBlock* ifFalse)
    : lhs(lhs), ifTrue(ifTrue), ifFalse(ifFalse)
    {
        lhs->uses.push_back(this);
    }

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

struct AssignInst : public Instruction
{
    AssignInst(Value* lhs, Value* rhs)
    : lhs(lhs), rhs(rhs)
    {
        assert(!lhs->definition);
        lhs->definition = this;

        rhs->uses.push_back(this);
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

struct ReturnInst : public Instruction
{
    ReturnInst(Value* value = nullptr)
    : value(value)
    {
        if (value)
            value->uses.push_back(this);
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

struct JumpInst : public Instruction
{
    JumpInst(BasicBlock* target)
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

struct CallInst : public Instruction
{
    CallInst(bool foreign, Value* dest, Value* function, const std::vector<Value*>& params = {}, bool ccall = false)
    : foreign(foreign), dest(dest), function(function), params(params), ccall(ccall)
    {
        if (dest)
        {
            assert(!dest->definition);
            dest->definition = this;
        }

        function->uses.push_back(this);

        for (auto& param : params)
        {
            param->uses.push_back(this);
        }
    }

    CallInst(bool foreign, Value* dest, Value* function, std::initializer_list<Value*> paramsList, bool ccall = false)
    : foreign(foreign), dest(dest), function(function), ccall(ccall)
    {
        if (dest)
        {
            assert(!dest->definition);
            dest->definition = this;
        }

        function->uses.push_back(this);

        for (auto& param : paramsList)
        {
            params.push_back(param);
            param->uses.push_back(this);
        }
    }

    CallInst(bool foreign, Value* function, std::initializer_list<Value*> paramsList = {}, bool ccall = false)
    : foreign(foreign), function(function), ccall(ccall)
    {
        for (auto& param : paramsList)
        {
            params.push_back(param);
            param->uses.push_back(this);
        }
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

struct IndirectCallInst : public Instruction
{
    IndirectCallInst(Value* dest, Value* function, const std::vector<Value*>& params)
    : dest(dest), function(function), params(params)
    {
        assert(!dest->definition);
        dest->definition = this;

        function->uses.push_back(this);
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

struct LoadInst : public Instruction
{
    LoadInst(Value* dest, Value* src)
    : dest(dest), src(src)
    {
        assert(!dest->definition);
        dest->definition = this;

        src->uses.push_back(this);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << dest->str() << " = [" << src->str() << "]";
        return ss.str();
    }

    Value* dest;
    Value* src;
};

struct StoreInst : public Instruction
{
    StoreInst(Value* dest, Value* src)
    : dest(dest), src(src)
    {
        dest->uses.push_back(this);
        src->uses.push_back(this);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "[" << dest->str() << "] = " << src->str();
        return ss.str();
    }

    Value* dest;
    Value* src;
};

struct IndexedLoadInst : public Instruction
{
    IndexedLoadInst(Value* lhs, Value* rhs, int64_t offset,
                    Value* index = nullptr, int64_t scale = 1)
    : lhs(lhs), rhs(rhs), offset(offset), index(index), scale(scale)
    {
        assert(!lhs->definition);
        lhs->definition = this;

        rhs->uses.push_back(this);
        if (index)
            index->uses.push_back(this);
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

struct IndexedStoreInst : public Instruction
{
    IndexedStoreInst(Value* lhs, size_t offset, Value* rhs)
    : lhs(lhs), offset(offset), rhs(rhs)
    {
        lhs->uses.push_back(this);
        rhs->uses.push_back(this);
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

struct BinaryOperationInst : public Instruction
{
    BinaryOperationInst(Value* dest, Value* lhs, BinaryOperation op, Value* rhs)
    : dest(dest), lhs(lhs), op(op), rhs(rhs)
    {
        assert(!dest->definition);
        dest->definition = this;

        lhs->uses.push_back(this);
        rhs->uses.push_back(this);
    }

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

struct UnreachableInst : public Instruction
{
    UnreachableInst()
    {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        return "unreachable";
    }
};

class PhiInst : public Instruction
{
public:
    PhiInst(Value* dest)
    : dest(dest)
    {
        assert(!dest->definition);
        dest->definition = this;
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << dest->str() << " = phi";

        for (size_t i = 0; i < _sources.size(); ++i)
        {
            if (i == 0)
            {
                ss << " ";
            }
            else
            {
                ss << ", ";
            }

            ss << "(" << _sources[i].first->str()
               << ", " << _sources[i].second->str() << ")";
        }

        return ss.str();
    }

    void addSource(BasicBlock* block, Value* value)
    {
        value->uses.push_back(this);
        _sources.emplace_back(block, value);
    }

    std::vector<std::pair<BasicBlock*, Value*>> sources()
    {
        return _sources;
    }

    Value* dest;

private:
    std::vector<std::pair<BasicBlock*, Value*>> _sources;
};

#endif
