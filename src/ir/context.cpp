#include "ir/context.hpp"

TACContext::TACContext()
{
    True = getConstantInt(1);
    False = getConstantInt(0);
    One = getConstantInt(1);
    Zero = getConstantInt(0);
}

TACContext::~TACContext()
{
    for (Value* value : _values)
    {
        delete value;
    }
}

Argument* TACContext::createArgument(ValueKind type, const std::string& name)
{
    Argument* result = new Argument(this, type, name);
    _values.push_back(result);
    return result;
}

ConstantInt* TACContext::getConstantInt(int64_t value)
{
    auto i = _constants.find(value);
    if (i != _constants.end())
    {
        return i->second;
    }
    else
    {
        ConstantInt* result = createConstantInt(value);
        _constants[value] = result;
        return result;
    }
}

ConstantInt* TACContext::createConstantInt(int64_t value)
{
    ConstantInt* result = new ConstantInt(this, value);
    _values.push_back(result);
    return result;
}

Function* TACContext::createExternFunction(const std::string& name)
{
    Function* result = new Function(this, name);
    _values.push_back(result);
    externs.push_back(result);
    return result;
}

Function* TACContext::createFunction(const std::string& name)
{
    Function* result = new Function(this, name);
    _values.push_back(result);
    functions.push_back(result);
    return result;
}

GlobalValue* TACContext::createGlobal(ValueKind type, const std::string& name)
{
    GlobalValue* result = new GlobalValue(this, type, name, GlobalTag::Variable);
    _values.push_back(result);
    globals.push_back(result);
    return result;
}

GlobalValue* TACContext::createStaticString(const std::string& name, const std::string& contents)
{
    GlobalValue* result = new GlobalValue(this, ValueKind::ReferenceType, name, GlobalTag::Static);
    _values.push_back(result);
    staticStrings.emplace_back(result, contents);
    return result;
}

LocalValue* TACContext::createLocal(ValueKind type, const std::string& name)
{
    LocalValue* result = new LocalValue(this, type, name);
    _values.push_back(result);
    return result;
}

Value* TACContext::createTemp(ValueKind type, int64_t number)
{
    Value* result = new Value(this, type, number);
    _values.push_back(result);
    return result;
}

Value* TACContext::createTemp(ValueKind type, const std::string& name)
{
    Value* result = new Value(this, type, name);
    _values.push_back(result);
    return result;
}

BasicBlock* TACContext::createBlock(Function* parent, int64_t number)
{
    BasicBlock* result = new BasicBlock(this, parent, number);
    _values.push_back(result);
    return result;
}