#include "context.hpp"

TACContext::TACContext()
{
    True = getConstantInt(3);
    False = getConstantInt(1);
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

Argument* TACContext::makeArgument(const std::string& name)
{
    Argument* result = new Argument(this, name);
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
        ConstantInt* result = makeConstantInt(value);
        _constants[value] = result;
        return result;
    }
}

ConstantInt* TACContext::makeConstantInt(int64_t value)
{
    ConstantInt* result = new ConstantInt(this, value);
    _values.push_back(result);
    return result;
}

Function* TACContext::makeExternFunction(const std::string& name)
{
    Function* result = new Function(this, name);
    _values.push_back(result);
    externs.push_back(result);
    return result;
}

Function* TACContext::makeFunction(const std::string& name)
{
    Function* result = new Function(this, name);
    _values.push_back(result);
    functions.push_back(result);
    return result;
}

GlobalValue* TACContext::makeGlobal(const std::string& name)
{
    GlobalValue* result = new GlobalValue(this, name, GlobalTag::Variable);
    _values.push_back(result);
    globals.push_back(result);
    return result;
}

GlobalValue* TACContext::makeStaticString(const std::string& name, const std::string& contents)
{
    GlobalValue* result = new GlobalValue(this, name, GlobalTag::Static);
    _values.push_back(result);
    staticStrings.emplace_back(result, contents);
    return result;
}

LocalValue* TACContext::makeLocal(const std::string& name)
{
    LocalValue* result = new LocalValue(this, name);
    _values.push_back(result);
    return result;
}

Value* TACContext::makeTemp(int64_t number)
{
    Value* result = new Value(this, number);
    _values.push_back(result);
    return result;
}

BasicBlock* TACContext::makeBlock(int64_t number)
{
    BasicBlock* result = new BasicBlock(this, number);
    _values.push_back(result);
    return result;
}