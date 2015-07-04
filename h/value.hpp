#ifndef VALUE_HPP
#define VALUE_HPP

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

struct Instruction;
struct TACContext;

struct Value
{
    virtual ~Value() {}

    virtual std::string str() const;

    std::unordered_set<Instruction*> uses;

    // Optional
    std::string name;
    Instruction* definition = nullptr;

    int64_t seqNumber = -1;

    TACContext* context() { return _context; }

protected:
    friend class TACContext;

    Value(TACContext* context, const std::string& name)
    : name(name), _context(context)
    {}

    Value(TACContext* context, int64_t seqNumber = -1)
    : seqNumber(seqNumber), _context(context)
    {}

    TACContext* _context;
};

struct Constant : public Value
{
protected:
    friend class TACContext;

    Constant(TACContext* context)
    : Value(context)
    {}

    Constant(TACContext* context, const std::string& name)
    : Value(context, name)
    {}
};

struct ConstantInt : public Constant
{
    int64_t value;

    virtual std::string str() const;

protected:
    friend class TACContext;

    ConstantInt(TACContext* context, int64_t value)
    : Constant(context)
    , value(value)
    {}
};

enum class GlobalTag {Variable, Function, Static};

struct GlobalValue : public Constant
{
    virtual std::string str() const;

    GlobalTag tag;

protected:
    friend class TACContext;

    GlobalValue(TACContext* context, const std::string& name, GlobalTag tag);
};

struct LocalValue : public Constant
{
    virtual std::string str() const;

protected:
    friend class TACContext;

    LocalValue(TACContext* context, const std::string& name);
};

struct Argument : public Constant
{
    virtual std::string str() const;

protected:
    friend class TACContext;

    Argument(TACContext* context, const std::string& name)
    : Constant(context, name)
    {}
};

#endif

