#ifndef VALUE_HPP
#define VALUE_HPP

#include "ir/value_type.hpp"

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

    ValueType type;

    // Optional
    std::string name;
    Instruction* definition = nullptr;

    int64_t seqNumber = -1;

    TACContext* context() { return _context; }

protected:
    friend struct TACContext;

    Value(TACContext* context, ValueType type, const std::string& name)
    : type(type), name(name), _context(context)
    {}

    Value(TACContext* context, ValueType type, int64_t seqNumber = -1)
    : type(type), seqNumber(seqNumber), _context(context)
    {}

    TACContext* _context;
};

struct Constant : public Value
{
protected:
    friend struct TACContext;

    Constant(TACContext* context, ValueType type)
    : Value(context, type)
    {}

    Constant(TACContext* context, ValueType type, const std::string& name)
    : Value(context, type, name)
    {}
};

struct ConstantInt : public Constant
{
    int64_t value;

    virtual std::string str() const;

protected:
    friend struct TACContext;

    ConstantInt(TACContext* context, ValueType type, int64_t value)
    : Constant(context, type)
    , value(value)
    {}
};

enum class GlobalTag {Variable, Function, Static, ExternFunction};

struct GlobalValue : public Constant
{
    virtual std::string str() const;

    GlobalTag tag;

protected:
    friend struct TACContext;

    GlobalValue(TACContext* context, ValueType type, const std::string& name, GlobalTag tag);
};

struct LocalValue : public Constant
{
    virtual std::string str() const;

protected:
    friend struct TACContext;

    LocalValue(TACContext* context, ValueType type, const std::string& name);
};

struct Argument : public Constant
{
    virtual std::string str() const;

protected:
    friend struct TACContext;

    Argument(TACContext* context, ValueType type, const std::string& name)
    : Constant(context, type, name)
    {}
};

#endif

