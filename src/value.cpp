#include "value.hpp"
#include "symbol.hpp"
#include <sstream>
#include <string>

std::string Value::str() const
{
    std::stringstream ss;
    ss << "%";

    if (seqNumber >= 0)
    {
        ss << seqNumber;
    }
    else
    {
        ss << name;
    }

    return ss.str();
}

std::string ConstantInt::str() const
{
    return std::to_string(value);
}

GlobalValue::GlobalValue(const std::string& name, GlobalTag tag)
: Constant(name), tag(tag)
{}

std::string GlobalValue::str() const
{
    std::stringstream ss;
    ss << "@" << name;
    return ss.str();
}

ConstantInt* ConstantInt::True = new ConstantInt(3);
ConstantInt* ConstantInt::False = new ConstantInt(1);

ConstantInt* ConstantInt::One = new ConstantInt(1);
ConstantInt* ConstantInt::Zero = new ConstantInt(0);

