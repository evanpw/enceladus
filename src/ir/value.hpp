#ifndef VALUE_HPP
#define VALUE_HPP

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

struct Instruction;
struct TACContext;

enum class ValueKind { ReferenceType, ValueType, CodeAddress };

static inline const char* valueTypeString(ValueKind type)
{
    switch (type)
    {
        case ValueKind::ReferenceType:
            return "ReferenceType";

        case ValueKind::ValueType:
            return "Integer";

        case ValueKind::CodeAddress:
            return "CodeAddress";
    }

    assert(false);
}

struct Value
{
    virtual ~Value() {}

    virtual std::string str() const;

    std::unordered_set<Instruction*> uses;

    ValueKind type;

    // Optional
    std::string name;
    Instruction* definition = nullptr;

    int64_t seqNumber = -1;

    TACContext* context() { return _context; }

protected:
    friend struct TACContext;

    Value(TACContext* context, ValueKind type, const std::string& name)
    : type(type), name(name), _context(context)
    {}

    Value(TACContext* context, ValueKind type, int64_t seqNumber = -1)
    : type(type), seqNumber(seqNumber), _context(context)
    {}

    TACContext* _context;
};

struct Constant : public Value
{
protected:
    friend struct TACContext;

    Constant(TACContext* context, ValueKind type)
    : Value(context, type)
    {}

    Constant(TACContext* context, ValueKind type, const std::string& name)
    : Value(context, type, name)
    {}
};

struct ConstantInt : public Constant
{
    int64_t value;

    virtual std::string str() const;

protected:
    friend struct TACContext;

    ConstantInt(TACContext* context, int64_t value)
    : Constant(context, ValueKind::ValueType)
    , value(value)
    {}
};

enum class GlobalTag {Variable, Function, Static};

struct GlobalValue : public Constant
{
    virtual std::string str() const;

    GlobalTag tag;

protected:
    friend struct TACContext;

    GlobalValue(TACContext* context, ValueKind type, const std::string& name, GlobalTag tag);
};

struct LocalValue : public Constant
{
    virtual std::string str() const;

protected:
    friend struct TACContext;

    LocalValue(TACContext* context, ValueKind type, const std::string& name);
};

struct Argument : public Constant
{
    virtual std::string str() const;

protected:
    friend struct TACContext;

    Argument(TACContext* context, ValueKind type, const std::string& name)
    : Constant(context, type, name)
    {}
};

#endif

