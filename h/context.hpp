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

    TACContext();
    ~TACContext();

    Argument* makeArgument(ValueType type, const std::string& name);
    ConstantInt* getConstantInt(int64_t value);
    Function* makeExternFunction(const std::string& name);
    Function* makeFunction(const std::string& name);
    GlobalValue* makeGlobal(ValueType type, const std::string& name);
    GlobalValue* makeStaticString(const std::string& name, const std::string& contents);
    LocalValue* makeLocal(ValueType type, const std::string& name);
    Value* makeTemp(ValueType type, int64_t number);
    Value* makeTemp(ValueType type, const std::string& name);
    BasicBlock* makeBlock(Function* parent, int64_t number);

    // Convenience references
    ConstantInt* True;
    ConstantInt* False;
    ConstantInt* One;
    ConstantInt* Zero;

private:
    ConstantInt* makeConstantInt(int64_t value);

    // Interning of constant values
    std::unordered_map<int64_t, ConstantInt*> _constants;

    // This contains every value, will have overlaps with members above
    std::vector<Value*> _values;
};

#endif