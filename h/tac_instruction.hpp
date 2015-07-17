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

    // Remove this instruction as the definition or user of any values, usually
    // in preparation for removing it
    virtual void dropReferences() = 0;

    virtual void replaceReferences(Value* from, Value* to) = 0;

    // Insert self before the given instruction
    void insertBefore(Instruction* inst)
    {
        assert(inst && inst->parent);
        parent = inst->parent;

        Instruction* instPrev = inst->prev;

        inst->prev = this;
        this->next = inst;

        this->prev = instPrev;
        if (instPrev)
        {
            instPrev->next = this;
        }
        else
        {
            assert(inst == parent->first);
            parent->first = this;
        }
    }

    // Insert self after the given instruction
    void insertAfter(Instruction* inst)
    {
        assert(inst && inst->parent);
        parent = inst->parent;

        Instruction* instNext = inst->next;

        inst->next = this;
        this->prev = inst;

        this->next = instNext;
        if (instNext)
        {
            instNext->prev = this;
        }
        else
        {
            assert(inst == parent->last);
            parent->last = this;
        }
    }

    void replaceWith(Instruction* inst)
    {
        assert(parent);

        if (prev)
        {
            prev->next = inst;
        }
        else
        {
            assert(parent->first == this);
            parent->first = inst;
        }

        if (next)
        {
            next->prev = inst;
        }
        else
        {
            assert(parent->last == this);
            parent->last = inst;
        }

        inst->parent = parent;
        inst->next = next;
        inst->prev = prev;

        dropReferences();
        delete this;
    }

    void removeFromParent()
    {
        assert(parent);

        if (prev)
        {
            prev->next = next;
        }
        else
        {
            assert(parent->first == this);
            parent->first = next;
        }

        if (next)
        {
            next->prev = prev;
        }
        else
        {
            assert(parent->last == this);
            parent->last = prev;
        }

        dropReferences();
        delete this;
    }

    BasicBlock* parent = nullptr;

    // Instructions in a single basic block form an instrusive linked list
    Instruction* prev = nullptr;
    Instruction* next = nullptr;

protected:
    void replaceReference(Value*& reference, Value* from, Value* to)
    {
        if (reference == from)
        {
            reference = to;
            from->uses.erase(this);
            to->uses.insert(this);
        }
    }
};

#define MAKE_VISITABLE() virtual void accept(TACVisitor* visitor) { visitor->visit(this); }

struct ConditionalJumpInst : public Instruction
{
    ConditionalJumpInst(Value* lhs, const std::string& op, Value* rhs, BasicBlock* ifTrue, BasicBlock* ifFalse)
    : lhs(lhs), op(op), rhs(rhs), ifTrue(ifTrue), ifFalse(ifFalse)
    {
        lhs->uses.insert(this);
        rhs->uses.insert(this);
    }

    virtual void dropReferences()
    {
        lhs->uses.erase(this);
        rhs->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        replaceReference(lhs, from, to);
        replaceReference(rhs, from, to);
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
        lhs->uses.insert(this);
    }

    virtual void dropReferences()
    {
        lhs->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        replaceReference(lhs, from, to);
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

struct ReturnInst : public Instruction
{
    ReturnInst(Value* value = nullptr)
    : value(value)
    {
        if (value)
            value->uses.insert(this);
    }

    virtual void dropReferences()
    {
        if (value)
            value->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        replaceReference(value, from, to);
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

    virtual void dropReferences() {}
    virtual void replaceReferences(Value* from, Value* to) {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << "jump " << target->str();
        return ss.str();
    }

    BasicBlock* target;
};

struct CopyInst : public Instruction
{
    CopyInst(Value* dest, Value* src)
    : dest(dest), src(src)
    {
        dest->definition = this;
        src->uses.insert(this);
    }

    virtual void dropReferences()
    {
        if (dest && this == dest->definition)
            dest->definition = nullptr;

        src->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        assert(dest != from);
        replaceReference(src, from, to);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << dest->str() << " = " << src->str();
        return ss.str();
    }

    Value* dest;
    Value* src;
};

struct UntagInst : public Instruction
{
    UntagInst(Value* dest, Value* src)
    : dest(dest), src(src)
    {
        dest->definition = this;
        src->uses.insert(this);
    }

    virtual void dropReferences()
    {
        if (dest && this == dest->definition)
            dest->definition = nullptr;

        src->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        assert(dest != from);
        replaceReference(src, from, to);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << dest->str() << " = untag " << src->str();
        return ss.str();
    }

    Value* dest;
    Value* src;
};

struct TagInst : public Instruction
{
    TagInst(Value* dest, Value* src)
    : dest(dest), src(src)
    {
        dest->definition = this;
        src->uses.insert(this);
    }

    virtual void dropReferences()
    {
        if (dest && this == dest->definition)
            dest->definition = nullptr;

        src->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        assert(dest != from);
        replaceReference(src, from, to);
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;
        ss << dest->str() << " = tag " << src->str();
        return ss.str();
    }

    Value* dest;
    Value* src;
};

struct CallInst : public Instruction
{
    CallInst(Value* dest, Value* function, const std::vector<Value*>& params = {})
    : dest(dest), function(function), params(params)
    {
        if (dest)
        {
            dest->definition = this;
        }

        function->uses.insert(this);

        for (auto& param : params)
        {
            param->uses.insert(this);
        }
    }

    CallInst(Value* dest, Value* function, std::initializer_list<Value*> paramsList)
    : dest(dest), function(function)
    {
        if (dest)
        {
            dest->definition = this;
        }

        function->uses.insert(this);

        for (auto& param : paramsList)
        {
            params.push_back(param);
            param->uses.insert(this);
        }
    }

    CallInst(Value* function, std::initializer_list<Value*> paramsList = {})
    : function(function)
    {
        for (auto& param : paramsList)
        {
            params.push_back(param);
            param->uses.insert(this);
        }
    }

    virtual void dropReferences()
    {
        if (dest && this == dest->definition)
            dest->definition = nullptr;

        function->uses.erase(this);

        for (auto& param : params)
        {
            param->uses.erase(this);
        }
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        assert(dest != from);

        replaceReference(function, from, to);

        for (size_t i = 0; i < params.size(); ++i)
        {
            replaceReference(params[i], from, to);
        }
    }

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        std::stringstream ss;

        if (dest)
        {
            ss << dest->str() << " = ";
        }

        ss << "call " << function->str() << "(";

        for (size_t i = 0; i < params.size(); ++i)
        {
            ss << params[i]->str();
            if (i + 1 != params.size()) ss << ", ";
        }
        ss << ")";

        return ss.str();
    }

    bool foreign = false;
    bool ccall = false;
    bool regpass = false;

    Value* dest;
    Value* function;
    std::vector<Value*> params;
};

struct LoadInst : public Instruction
{
    LoadInst(Value* dest, Value* src)
    : dest(dest), src(src)
    {
        dest->definition = this;

        src->uses.insert(this);
    }

    virtual void dropReferences()
    {
        if (this == dest->definition)
            dest->definition = nullptr;

        src->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        assert(dest != from);

        replaceReference(src, from, to);
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
        dest->uses.insert(this);
        src->uses.insert(this);
    }

    virtual void dropReferences()
    {
        dest->uses.erase(this);
        src->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        replaceReference(dest, from, to);
        replaceReference(src, from, to);
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
    IndexedLoadInst(Value* lhs, Value* rhs, int64_t offset)
    : lhs(lhs), rhs(rhs), offset(offset)
    {
        lhs->definition = this;

        rhs->uses.insert(this);
    }

    virtual void dropReferences()
    {
        if (this == lhs->definition)
            lhs->definition = nullptr;

        rhs->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        assert(lhs != from);

        replaceReference(rhs, from, to);
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
};

struct IndexedStoreInst : public Instruction
{
    IndexedStoreInst(Value* lhs, size_t offset, Value* rhs)
    : lhs(lhs), offset(offset), rhs(rhs)
    {
        lhs->uses.insert(this);
        rhs->uses.insert(this);
    }

    virtual void dropReferences()
    {
        lhs->uses.erase(this);
        rhs->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        replaceReference(lhs, from, to);
        replaceReference(rhs, from, to);
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

enum class BinaryOperation {ADD, SUB, MUL, DIV, MOD, AND, SHR, SHL};
extern const char* binaryOperationNames[];

struct BinaryOperationInst : public Instruction
{
    BinaryOperationInst(Value* dest, Value* lhs, BinaryOperation op, Value* rhs)
    : dest(dest), lhs(lhs), op(op), rhs(rhs)
    {
        dest->definition = this;

        lhs->uses.insert(this);
        rhs->uses.insert(this);
    }

    virtual void dropReferences()
    {
        if (this == dest->definition)
            dest->definition = nullptr;

        lhs->uses.erase(this);
        rhs->uses.erase(this);
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        assert(dest != from);
        replaceReference(lhs, from, to);
        replaceReference(rhs, from, to);
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
    UnreachableInst() {}

    virtual void dropReferences() {}
    virtual void replaceReferences(Value* from, Value* to) {}

    MAKE_VISITABLE();

    virtual std::string str() const override
    {
        return "unreachable";
    }
};

struct PhiInst : public Instruction
{
    PhiInst(Value* dest)
    : dest(dest)
    {
        dest->definition = this;
    }

    virtual void dropReferences()
    {
        if (this == dest->definition)
            dest->definition = nullptr;

        for (auto& item : _sources)
        {
            if (item.second)
                item.second->uses.erase(this);
        }
    }

    virtual void replaceReferences(Value* from, Value* to)
    {
        assert(dest != from);

        for (auto& item : _sources)
        {
            replaceReference(item.second, from, to);
        }
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

            ss << "("
               << _sources[i].first->str()
               << ", "
               << (_sources[i].second ? _sources[i].second->str() : "null")
               << ")";
        }

        return ss.str();
    }

    void addSource(BasicBlock* block, Value* value)
    {
        if (value)
            value->uses.insert(this);

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
