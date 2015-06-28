#include "context.hpp"

TACContext::~TACContext()
{
    for (Value* value : _values)
    {
        delete value;
    }
}

Argument* TACContext::makeArgument(const std::string& name)
{
    Argument* result = new Argument(name);
    _values.push_back(result);
    return result;
}

ConstantInt* TACContext::makeConstantInt(int64_t value)
{
    ConstantInt* result = new ConstantInt(value);
    _values.push_back(result);
    return result;
}

Function* TACContext::makeExternFunction(const std::string& name)
{
    Function* result = new Function(name);
    _values.push_back(result);
    externs.push_back(result);
    return result;
}

Function* TACContext::makeFunction(const std::string& name)
{
    Function* result = new Function(name);
    _values.push_back(result);
    functions.push_back(result);
    return result;
}

GlobalValue* TACContext::makeGlobal(const std::string& name)
{
    GlobalValue* result = new GlobalValue(name, GlobalTag::Variable);
    _values.push_back(result);
    globals.push_back(result);
    return result;
}

GlobalValue* TACContext::makeStaticString(const std::string& name, const std::string& contents)
{
    GlobalValue* result = new GlobalValue(name, GlobalTag::Static);
    _values.push_back(result);
    staticStrings.emplace_back(result, contents);
    return result;
}

LocalValue* TACContext::makeLocal(const std::string& name)
{
    LocalValue* result = new LocalValue(name);
    _values.push_back(result);
    return result;
}

Value* TACContext::makeTemp(int64_t number)
{
    Value* result = new Value(number);
    _values.push_back(result);
    return result;
}

BasicBlock* TACContext::makeBlock(int64_t number)
{
    BasicBlock* result = new BasicBlock(number);
    _values.push_back(result);
    return result;
}