#include "ir/value.hpp"
#include "semantic/symbol.hpp"

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

GlobalValue::GlobalValue(TACContext* context, ValueType type, const std::string& name, GlobalTag tag)
: Constant(context, type, name), tag(tag)
{}

std::string GlobalValue::str() const
{
    std::stringstream ss;
    ss << "@" << name;
    return ss.str();
}

LocalValue::LocalValue(TACContext* context, ValueType type, const std::string& name)
: Constant(context, type, name)
{}

std::string LocalValue::str() const
{
    std::stringstream ss;
    ss << "$" << name;
    return ss.str();
}

std::string Argument::str() const
{
    std::stringstream ss;
    ss << "$" << name;
    return ss.str();
}
