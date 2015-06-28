#ifndef VALUE_HPP
#define VALUE_HPP

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

struct Instruction;

struct Value
{
    Value(const std::string& name)
    : name(name)
    {}

    Value(int64_t seqNumber = -1)
    : seqNumber(seqNumber)
    {}

    virtual ~Value() {}

    virtual std::string str() const;

    std::vector<Instruction*> uses;

    // Optional
    std::string name;
    Instruction* definition = nullptr;

    int64_t seqNumber = -1;
};

struct Constant : public Value
{
    Constant() {}

    Constant(const std::string& name)
    : Value(name)
    {}
};

struct ConstantInt : public Constant
{
    ConstantInt(int64_t value)
    : value(value)
    {}

    int64_t value;

    virtual std::string str() const;

    static ConstantInt* True;
    static ConstantInt* False;
    static ConstantInt* Zero;
    static ConstantInt* One;
};

enum class GlobalTag {Variable, Function, Static};

struct GlobalValue : public Constant
{
    GlobalValue(const std::string& name, GlobalTag tag);

    virtual std::string str() const;

    GlobalTag tag;
};

struct LocalValue : public Constant
{
    LocalValue(const std::string& name);

    virtual std::string str() const;
};

struct Argument : public Constant
{
    Argument(const std::string& name)
    : Constant(name)
    {}

    virtual std::string str() const;
};

#endif

