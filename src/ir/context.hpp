#ifndef TAC_CONTEXT_HPP
#define TAC_CONTEXT_HPP

#include "ir/function.hpp"
#include "ir/tac_instruction.hpp"
#include "ir/value.hpp"
#include "semantic/symbol.hpp"

#include <vector>

struct TACContext
{
    std::vector<Function*> functions;
    std::vector<Value*> globals;
    std::vector<std::pair<Value*, std::string>> staticStrings;
    std::vector<Value*> externs;

    TACContext();
    ~TACContext();

    Argument* createArgument(ValueType type, const std::string& name);
    ConstantInt* getConstantInt(int64_t value);
    Function* createExternFunction(const std::string& name);
    Function* createFunction(const std::string& name);
    GlobalValue* createGlobal(ValueType type, const std::string& name);
    GlobalValue* createStaticString(const std::string& name, const std::string& contents);
    LocalValue* createLocal(ValueType type, const std::string& name);
    Value* createTemp(ValueType type, int64_t number);
    Value* createTemp(ValueType type, const std::string& name);
    BasicBlock* createBlock(Function* parent, int64_t number);

    // Convenience references
    ConstantInt* True;
    ConstantInt* False;
    ConstantInt* One;
    ConstantInt* Zero;

private:
    ConstantInt* createConstantInt(int64_t value);

    // Interning of constant values
    std::unordered_map<int64_t, ConstantInt*> _constants;

    // This contains every value, will have overlaps with members above
    std::vector<Value*> _values;
};

#endif