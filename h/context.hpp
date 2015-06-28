#ifndef TAC_CONTEXT_HPP
#define TAC_CONTEXT_HPP

#include "function.hpp"
#include "symbol.hpp"
#include "tac_instruction.hpp"
#include "value.hpp"

#include <vector>

struct TACContext
{
    std::vector<Function*> functions;
    std::vector<Value*> globals;
    std::vector<std::pair<Value*, std::string>> staticStrings;
    std::vector<Value*> externs;

    ~TACContext();

    Argument* makeArgument(const std::string& name);
    ConstantInt* makeConstantInt(int64_t value);
    Function* makeExternFunction(const std::string& name);
    Function* makeFunction(const std::string& name);
    GlobalValue* makeGlobal(const std::string& name);
    GlobalValue* makeStaticString(const std::string& name, const std::string& contents);
    LocalValue* makeLocal(const std::string& name);
    Value* makeTemp(int64_t number);
    BasicBlock* makeBlock(int64_t number);

private:
    // This contains every value, will have overlaps with members above
    std::vector<Value*> _values;
};

#endif