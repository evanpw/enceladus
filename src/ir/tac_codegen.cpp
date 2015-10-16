#include "ir/tac_codegen.hpp"

#include "ast/ast_context.hpp"
#include "lib/library.h"
#include "semantic/type_functions.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>

TACCodeGen::TACCodeGen(TACContext* context)
: _context(context), _conditionalCodeGen(this)
{
}

void TACCodeGen::codeGen(AstContext* astContext)
{
    _astContext = astContext;
     astContext->root()->accept(this);
}

TACConditionalCodeGen::TACConditionalCodeGen(TACCodeGen* mainCodeGen)
: _mainCodeGen(mainCodeGen)
{
    _context = _mainCodeGen->_context;
}

static std::string mangleTypeName(Type* type, std::vector<TypeVariable*>& variables)
{
    std::stringstream ss;

    if (BaseType* baseType = type->get<BaseType>())
    {
        std::string name = baseType->str();
        ss << name.size() << name;
    }
    else if (FunctionType* functionType = type->get<FunctionType>())
    {
        ss << "8Function" << "L";
        for (auto& input : functionType->inputs())
        {
            ss << mangleTypeName(input, variables);
        }
        ss << mangleTypeName(functionType->output(), variables);
        ss << "G";
    }
    else if (ConstructedType* constructedType = type->get<ConstructedType>())
    {
        std::string name = constructedType->name();
        ss << name.size() << name << "L";
        for (auto& param : constructedType->typeParameters())
        {
            ss << mangleTypeName(param, variables);
        }
        ss << "G";
    }
    else /* TypeVariable */
    {
        assert(false);
    }

    ss << "E";

    return ss.str();
}

static std::string mangleTypeName(Type* type)
{
    std::vector<TypeVariable*> variables;
    return mangleTypeName(type, variables);
}

Value* TACCodeGen::getValue(const Symbol* symbol)
{
    if (!symbol)
    {
        return nullptr;
    }

    std::unordered_map<const Symbol*, Value*>::iterator i;
    if ((i = _globalNames.find(symbol)) != _globalNames.end())
    {
        return i->second;
    }
    else if ((i = _localNames.find(symbol)) != _localNames.end())
    {
        return i->second;
    }

    assert(symbol->kind == kVariable);

    const VariableSymbol* varSymbol = dynamic_cast<const VariableSymbol*>(symbol);
    if (varSymbol->isStatic)
    {
        Value* result = _context->createStaticString(symbol->name, varSymbol->contents);
        _globalNames[symbol] = result;
        return result;
    }
    else if (symbol->global)
    {
        ValueType type = getValueType(symbol->type);
        Value* result = _context->createGlobal(type, symbol->name);
        _globalNames[symbol] = result;
        return result;
    }
    else if (varSymbol->isParam)
    {
        ValueType type = getValueType(symbol->type);
        Value* result = _context->createArgument(type, symbol->name);
        _localNames[symbol] = result;
        return result;
    }
    else
    {
        ValueType type = getValueType(symbol->type);
        Value* result = _context->createLocal(type, symbol->name);
        _currentFunction->locals.push_back(result);
        _localNames[symbol] = result;
        return result;
    }
}

static bool subAssignment(const TypeAssignment& lhs, const TypeAssignment& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (auto& assignment : lhs)
    {
        auto match = rhs.find(assignment.first);
        if (match == rhs.end())
            return false;

        if (!isSubtype(assignment.second, match->second))
        {
            return false;
        }
    }

    return true;
}

static bool sameAssignment(const TypeAssignment& lhs, const TypeAssignment& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return subAssignment(lhs, rhs) || subAssignment(rhs, lhs);
}

static bool isConcrete(Type* original)
{
    switch (original->tag())
    {
        case ttBase:
            return true;

        case ttVariable:
            return false;

        case ttFunction:
        {
            FunctionType* functionType = original->get<FunctionType>();

            for (auto& input : functionType->inputs())
            {
                if (!isConcrete(input))
                    return false;
            }

            if (!isConcrete(functionType->output()))
                return false;

            return true;
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = original->get<ConstructedType>();

            for (auto& param : constructedType->typeParameters())
            {
                if (!isConcrete(param))
                    return false;
            }

            return true;
        }
    }

    assert(false);
}

static Type* substitute(Type* original, const TypeAssignment& typeAssignment)
{
    switch (original->tag())
    {
        case ttBase:
            return original;

        case ttVariable:
        {
            TypeVariable* typeVariable = original->get<TypeVariable>();

            auto i = typeAssignment.find(typeVariable);
            if (i != typeAssignment.end())
            {
                return i->second;
            }
            else
            {
                return original;
            }
        }

        case ttFunction:
        {
            FunctionType* functionType = original->get<FunctionType>();

            bool changed = false;
            std::vector<Type*> newInputs;
            for (auto& input : functionType->inputs())
            {
                Type* newInput = substitute(input, typeAssignment);
                changed |= (newInput != input);
                newInputs.push_back(newInput);
            }

            Type* newOutput = substitute(functionType->output(), typeAssignment);
            changed |= (newOutput != functionType->output());

            if (changed)
            {
                TypeTable* typeTable = original->table();
                return typeTable->createFunctionType(newInputs, newOutput);
            }
            else
            {
                return original;
            }
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = original->get<ConstructedType>();

            bool changed = false;
            std::vector<Type*> newParams;
            for (auto& param : constructedType->typeParameters())
            {
                Type* newParam = substitute(param, typeAssignment);
                changed |= (newParam != param);
                newParams.push_back(newParam);
            }

            if (changed)
            {
                TypeTable* typeTable = original->table();
                return constructedType->prototype()->instantiate(std::move(newParams));
            }
            else
            {
                return original;
            }
        }
    }

    assert(false);
}

static TypeAssignment combine(const TypeAssignment& typeContext, const TypeAssignment& typeAssignment)
{
    TypeAssignment result = typeAssignment;

    for (auto& assignment : typeAssignment)
    {
        result[assignment.first] = substitute(assignment.second, typeContext);
    }

    return result;
}

static ValueType getRealValueType(Type* type)
{
    if (type->isBoxed())
    {
        return ValueType::Reference;
    }
    else
    {
        BaseType* baseType = type->get<BaseType>();
        assert(baseType);

        if (baseType->size() == 64)
        {
            if (baseType->isSigned())
            {
                return ValueType::I64;
            }
            else
            {
                return ValueType::U64;
            }
        }
        else if (baseType->size() == 8)
        {
            assert(!baseType->isSigned());
            return ValueType::U8;
        }

        assert(false);
    }
}

ValueType TACCodeGen::getValueType(Type* type, const TypeAssignment& typeAssignment)
{
    TypeAssignment fullAssignment = combine(_typeContext, typeAssignment);
    Type* realType = substitute(type, fullAssignment);

    return getRealValueType(realType);
}

ValueType TACCodeGen::getValueType(Type* type)
{
    Type* realType = substitute(type, _typeContext);
    return getRealValueType(realType);
}

size_t TACCodeGen::getNumPointers(const ConstructorSymbol* symbol, AstNode* node, const TypeAssignment& typeAssignment)
{
    // Perform the layout
    getConstructorLayout(symbol, node, typeAssignment);

    Function* function = (Function*)getFunctionValue(symbol, node, typeAssignment);
    return _constructorLayouts.at(function).first;
}

std::vector<size_t> TACCodeGen::getConstructorLayout(const ConstructorSymbol* symbol, AstNode* node, const TypeAssignment& typeAssignment)
{
    Function* function = (Function*)getFunctionValue(symbol, node, typeAssignment);
    auto i = _constructorLayouts.find(function);
    if (i != _constructorLayouts.end())
    {
        return i->second.second;
    }

    TypeAssignment realAssignment = combine(_typeContext, typeAssignment);

    for (auto& assignment : realAssignment)
    {
        if (!isConcrete(assignment.second))
        {
            assert(node);

            YYLTYPE location = node->location;
            std::stringstream ss;

            ss << location.filename << ":" << location.first_line << ":" << location.first_column
               << ": cannot infer concrete type of call to constructor " << symbol->name;

            throw CodegenError(ss.str());
        }
    }

    ValueConstructor* constructor = symbol->constructor;
    const std::vector<ValueConstructor::MemberDesc> members = constructor->members();

    std::vector<size_t> referenceMembers;
    std::vector<size_t> valueMembers;

    for (size_t i = 0; i < members.size(); ++i)
    {
        auto& member = members[i];

        Type* type = substitute(member.type, realAssignment);
        assert(isConcrete(type));

        if (type->isBoxed())
        {
            referenceMembers.push_back(i);
        }
        else
        {
            valueMembers.push_back(i);
        }
    }

    std::vector<size_t> memberOrder = referenceMembers;
    for (size_t index : valueMembers)
    {
        memberOrder.push_back(index);
    }

    _constructorLayouts.emplace(function, std::make_pair(referenceMembers.size(), memberOrder));
    return memberOrder;
}

Value* TACCodeGen::getFunctionValue(const Symbol* symbol, AstNode* node, const TypeAssignment& typeAssignment)
{
    auto& instantiations = _functionNames[symbol];

    TypeAssignment realAssignment = combine(_typeContext, typeAssignment);

    for (auto& assignment : realAssignment)
    {
        if (!isConcrete(assignment.second))
        {
            assert(node);

            YYLTYPE location = node->location;
            std::stringstream ss;

            ss << location.filename << ":" << location.first_line << ":" << location.first_column
               << ": cannot infer concrete type of call to function " << symbol->name;

            throw CodegenError(ss.str());
        }
    }

    for (auto& instantiation : instantiations)
    {
        if (sameAssignment(instantiation.first, realAssignment))
        {
            return instantiation.second;
        }
    }

    Function* result;

    if (symbol->kind == kFunction)
    {
        const FunctionSymbol* functionSymbol = dynamic_cast<const FunctionSymbol*>(symbol);
        if (functionSymbol->isExternal)
        {
            assert(instantiations.empty());

            auto i = _externFunctions.find(symbol);
            if (i != _externFunctions.end())
            {
                return i->second;
            }
            else
            {
                GlobalValue* result = _context->createExternFunction(symbol->name);
                _externFunctions.emplace(symbol, result);
                return result;
            }
        }
        else /* regular function or constructor */
        {
            std::stringstream ss;
            ss << symbol->name;

            if (!realAssignment.empty())
            {
                ss << "$A";
                for (auto& assignment : realAssignment)
                {
                    ss << mangleTypeName(assignment.second);
                }
            }

            result = _context->createFunction(ss.str());

            _functions.emplace_back(functionSymbol, realAssignment);
        }
    }
    else if (symbol->kind == kMethod)
    {
        const MethodSymbol* methodSymbol = dynamic_cast<const MethodSymbol*>(symbol);
        assert(methodSymbol);

        // We have to append a unique number to method names because several
        // types can have a method with the same name
        std::stringstream ss;
        ss << methodSymbol->name << "$M";

        if (BaseType* baseType = methodSymbol->parentType->get<BaseType>())
        {
            ss << baseType->str();
        }
        else if (ConstructedType* constructedType = methodSymbol->parentType->get<ConstructedType>())
        {
            ss << constructedType->name();
        }
        else if (TypeVariable* typeVariable = methodSymbol->parentType->get<TypeVariable>())
        {
            ss << "_";
            if (!typeVariable->constraints().empty())
            {
                ss << "L";
                for (const Trait* constraint : typeVariable->constraints())
                {
                    std::string name = constraint->str();
                    ss << name.size() << name;
                }
                ss << "G";
            }
        }
        else
        {
            assert(false);
        }

        if (!realAssignment.empty())
        {
            ss << "$A";
            for (auto& assignment : realAssignment)
            {
                ss << mangleTypeName(assignment.second);
            }
        }

        result = _context->createFunction(ss.str());
        _functions.emplace_back(methodSymbol, realAssignment);
    }
    else
    {
        assert(false);
    }

    instantiations.emplace_back(realAssignment, result);
    return result;
}

void TACConditionalCodeGen::emit(Instruction* inst)
{
    _mainCodeGen->emit(inst);
}

void TACCodeGen::emit(Instruction* inst)
{
    inst->parent = _currentBlock;
    _currentBlock->append(inst);
}

Value* TACConditionalCodeGen::visitAndGet(AstNode* node)
{
    return _mainCodeGen->visitAndGet(node);
}

void TACConditionalCodeGen::setBlock(BasicBlock* block)
{
    _mainCodeGen->setBlock(block);
}

BasicBlock* TACConditionalCodeGen::createBlock()
{
    return _mainCodeGen->createBlock();
}

static FunctionDefNode* getFunctionDefinition(const Symbol* symbol)
{
    if (const FunctionSymbol* functionSymbol = dynamic_cast<const FunctionSymbol*>(symbol))
    {
        return functionSymbol->definition;
    }
    else if (const MethodSymbol* methodSymbol = dynamic_cast<const MethodSymbol*>(symbol))
    {
        return methodSymbol->definition;
    }
    else
    {
        assert(false);
    }
}

void TACCodeGen::visit(ProgramNode* node)
{
    Function* main = _context->createFunction("splmain");
    _currentFunction = main;
    setBlock(createBlock());

    for (auto& child : node->children)
    {
        child->accept(this);
    }

    emit(new ReturnInst);

    // The previous loop will have filled in _functions with all
    // functions / methods visited from the top level. Recursively generate
    // code for all functions / methods reachable from those, and so on.
    while (!_functions.empty())
    {
        const Symbol* symbol = _functions.front().first;
        FunctionDefNode* funcDefNode = getFunctionDefinition(symbol);
        TypeAssignment typeContext = _functions.front().second;
        _functions.pop_front();

        if (funcDefNode) /* regular function or method */
        {
            Function* function = (Function*)getFunctionValue(funcDefNode->symbol, nullptr, typeContext);

            _currentFunction = function;
            _localNames.clear();
            _typeContext = typeContext;
            setBlock(createBlock());

            // Collect all function parameters
            for (Symbol* param : funcDefNode->parameterSymbols)
            {
                assert(dynamic_cast<VariableSymbol*>(param)->isParam);
                _currentFunction->params.push_back(getValue(param));
            }

            // Generate code for the function body
            funcDefNode->body->accept(this);

            // Handle implicit return values
            if (!_currentBlock->isTerminated())
            {
                emit(new ReturnInst(funcDefNode->body->value));
            }
        }
        else
        {
            const ConstructorSymbol* constructorSymbol = dynamic_cast<const ConstructorSymbol*>(symbol);
            assert(constructorSymbol);

            Function* function = (Function*)getFunctionValue(constructorSymbol, nullptr, typeContext);
            _currentFunction = function;
            setBlock(createBlock());

            _typeContext.clear();
            createConstructor(constructorSymbol, typeContext);
        }
    }
}

void TACConditionalCodeGen::visit(ComparisonNode* node)
{
    Value* lhs = visitAndGet(node->lhs);
    Value* rhs = visitAndGet(node->rhs);

    switch(node->op)
    {
        case ComparisonNode::kGreater:
            emit(new ConditionalJumpInst(lhs, ">", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kLess:
            emit(new ConditionalJumpInst(lhs, "<", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kEqual:
            emit(new ConditionalJumpInst(lhs, "==", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kGreaterOrEqual:
            emit(new ConditionalJumpInst(lhs, ">=", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kLessOrEqual:
            emit(new ConditionalJumpInst(lhs, "<=", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kNotEqual:
            emit(new ConditionalJumpInst(lhs, "!=", rhs, _trueBranch, _falseBranch));
            break;

        default:
            assert(false);
    }
}

void TACCodeGen::visit(ComparisonNode* node)
{
    std::string operation;
    switch(node->op)
    {
        case ComparisonNode::kGreater:
            operation = ">";
            break;

        case ComparisonNode::kLess:
            operation = "<";
            break;

        case ComparisonNode::kEqual:
            operation = "==";
            break;

        case ComparisonNode::kGreaterOrEqual:
            operation = ">=";
            break;

        case ComparisonNode::kLessOrEqual:
            operation = "<=";
            break;

        case ComparisonNode::kNotEqual:
            operation = "!=";
            break;

        default:
            assert(false);
    }

    Value* lhs = visitAndGet(node->lhs);
    Value* rhs = visitAndGet(node->rhs);

    BasicBlock* trueBranch = createBlock();
    BasicBlock* falseBranch = createBlock();
    BasicBlock* continueAt = createBlock();

    emit(new ConditionalJumpInst(lhs, operation, rhs, trueBranch, falseBranch));

    setBlock(falseBranch);
    emit(new JumpInst(continueAt));

    setBlock(trueBranch);
    emit(new JumpInst(continueAt));

    setBlock(continueAt);
    node->value = createTemp(ValueType::U64);
    PhiInst* phi = new PhiInst(node->value);
    phi->addSource(falseBranch, _context->False);
    phi->addSource(trueBranch, _context->True);
    emit(phi);
}

void TACConditionalCodeGen::visit(LogicalNode* node)
{
    if (node->op == LogicalNode::kAnd)
    {
        BasicBlock* firstTrue = createBlock();
        visitCondition(*node->lhs, firstTrue, _falseBranch);

        setBlock(firstTrue);
        visitCondition(*node->rhs, _trueBranch, _falseBranch);
    }
    else if (node->op == LogicalNode::kOr)
    {
        BasicBlock* firstFalse = createBlock();
        visitCondition(*node->lhs, _trueBranch, firstFalse);

        setBlock(firstFalse);
        visitCondition(*node->rhs, _trueBranch, _falseBranch);
    }
    else
    {
        assert(false);
    }
}

void TACCodeGen::visit(LogicalNode* node)
{
    BasicBlock* continueAt = createBlock();
    BasicBlock* testSecond = createBlock();
    BasicBlock* trueBranch = createBlock();
    BasicBlock* falseBranch = createBlock();

    if (node->op == LogicalNode::kAnd)
    {
        Value* lhs = visitAndGet(node->lhs);
        emit(new JumpIfInst(lhs, testSecond, falseBranch));

        setBlock(testSecond);
        Value* rhs = visitAndGet(node->rhs);
        emit(new JumpIfInst(rhs, trueBranch, falseBranch));
    }
    else if (node->op == LogicalNode::kOr)
    {
        Value* lhs = visitAndGet(node->lhs);
        emit(new JumpIfInst(lhs, trueBranch, testSecond));

        setBlock(testSecond);
        Value* rhs = visitAndGet(node->rhs);
        emit(new JumpIfInst(rhs, trueBranch, falseBranch));
    }
    else
    {
        assert (false);
    }

    setBlock(trueBranch);
    emit(new JumpInst(continueAt));

    setBlock(falseBranch);
    emit(new JumpInst(continueAt));

    setBlock(continueAt);
    node->value = createTemp(ValueType::U64);
    PhiInst* phi = new PhiInst(node->value);
    phi->addSource(falseBranch, _context->False);
    phi->addSource(trueBranch, _context->True);
    emit(phi);

}

void TACCodeGen::visit(NullaryNode* node)
{
    if (node->kind == NullaryNode::VARIABLE)
    {
        Value* rhs = getValue(node->symbol);
        node->value = createTemp(rhs->type);
        emit(new LoadInst(node->value, rhs));
    }
    else
    {
        ValueType type = getValueType(node->type, node->typeAssignment);
        Value* dest = node->value = createTemp(type);

        FunctionSymbol* functionSymbol = dynamic_cast<FunctionSymbol*>(node->symbol);

        if (node->kind == NullaryNode::FUNC_CALL)
        {
            Value* fn = getFunctionValue(node->symbol, node, node->typeAssignment);
            CallInst* inst = new CallInst(dest, fn);
            inst->ccall = functionSymbol->isExternal;
            inst->regpass = inst->ccall;
            emit(inst);
        }
        else
        {
            assert(node->kind == NullaryNode::CLOSURE);

            size_t size = sizeof(SplObject) + 8;
            CallInst* callInst = new CallInst(dest, _context->createExternFunction("gcAllocate"), {_context->createConstantInt(ValueType::U64, size)});
            callInst->regpass = true;
            emit(callInst);

            // SplObject header fields
            emit(new IndexedStoreInst(dest, _context->createConstantInt(ValueType::U64, offsetof(SplObject, constructorTag)), _context->Zero));
            emit(new IndexedStoreInst(dest, _context->createConstantInt(ValueType::U64, offsetof(SplObject, numReferences)), _context->Zero));

            // Address of the function as an unboxed member
            Value* fn = getFunctionValue(node->symbol, node, node->typeAssignment);
            emit(new IndexedStoreInst(dest, _context->createConstantInt(ValueType::U64, sizeof(SplObject)), fn));
        }
    }
}

void TACCodeGen::visit(IntNode* node)
{
    ValueType type = getValueType(node->type);
    node->value = _context->createConstantInt(type, node->intValue);
}

void TACCodeGen::visit(CastNode* node)
{
    Value* src = visitAndGet(node->lhs);
    node->value = createTemp(getValueType(node->type));

    emit(new CopyInst(node->value, src));
}

void TACCodeGen::visit(BoolNode* node)
{
    if (node->boolValue)
    {
        node->value = _context->True;
    }
    else
    {
        node->value = _context->False;
    }
}

void TACCodeGen::visit(BlockNode* node)
{
    for (auto& child : node->children)
    {
        child->accept(this);
    }

    if (node->children.size() > 0)
    {
        node->value = node->children.back()->value;
    }
}

void TACCodeGen::visit(IfNode* node)
{
    BasicBlock* trueBranch = createBlock();
    BasicBlock* continueAt = createBlock();

    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, continueAt);

    setBlock(trueBranch);
    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(continueAt));

    setBlock(continueAt);
}

void TACCodeGen::visit(IfElseNode* node)
{
    BasicBlock* trueBranch = createBlock();
    BasicBlock* falseBranch = createBlock();

    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, falseBranch);

    BasicBlock* continueAt = nullptr;

    setBlock(trueBranch);
    Value* bodyValue = visitAndGet(node->body);
    if (!_currentBlock->isTerminated())
    {
        continueAt = createBlock();

        trueBranch = _currentBlock;
        emit(new JumpInst(continueAt));
    }

    setBlock(falseBranch);
    Value* elseValue = visitAndGet(node->elseBody);
    if (!_currentBlock->isTerminated())
    {
        if (!continueAt)
            continueAt = createBlock();

        falseBranch = _currentBlock;
        emit(new JumpInst(continueAt));
    }

    if (continueAt)
    {
        setBlock(continueAt);
        if (bodyValue)
        {
            assert(elseValue);

            node->value = createTemp(bodyValue->type);
            PhiInst* phi = new PhiInst(node->value);
            phi->addSource(trueBranch, bodyValue);
            phi->addSource(falseBranch, elseValue);
            emit(phi);
        }
    }
}

void TACCodeGen::visit(AssertNode* node)
{
    static size_t counter = 1;

    // HACK
    Value* panicFunction = getFunctionValue(node->panicSymbol, node);

    BasicBlock* falseBranch = createBlock();
    BasicBlock* continueAt = createBlock();

    _conditionalCodeGen.visitCondition(*node->condition, continueAt, falseBranch);

    setBlock(falseBranch);

    // Create the assert failure message as a static string
    auto location = node->location;
    std::string name = "__assertMessage" + std::to_string(counter++);
    std::stringstream contents;
    contents << "*** Exception: Assertion failed at "
             << location.filename << ":"
             << location.first_line << ":"
             << location.first_column;

    Value* message = _context->createStaticString(name, contents.str());
    CallInst* inst = new CallInst(createTemp(ValueType::U64), panicFunction, {message});
    inst->ccall = true;
    inst->regpass = true;
    emit(inst);

    emit(new JumpInst(continueAt));
    setBlock(continueAt);
}

void TACCodeGen::visit(WhileNode* node)
{
    BasicBlock* loopBegin = createBlock();
    BasicBlock* loopExit = createBlock();

    emit(new JumpInst(loopBegin));
    setBlock(loopBegin);

    BasicBlock* loopBody = createBlock();
    _conditionalCodeGen.visitCondition(*node->condition, loopBody, loopExit);

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    _currentLoopExit = loopExit;

    setBlock(loopBody);
    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(loopBegin));

    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(ForeachNode* node)
{
    // HACK
    Value* headFunction = getFunctionValue(node->headSymbol, node, node->headTypeAssignment);
    Value* tailFunction = getFunctionValue(node->tailSymbol, node, node->tailTypeAssignment);
    Value* emptyFunction = getFunctionValue(node->emptySymbol, node, node->emptyTypeAssignment);

    BasicBlock* loopInit = createBlock();
    BasicBlock* loopBegin = createBlock();
    BasicBlock* loopExit = createBlock();
    BasicBlock* loopBody = createBlock();

    // Create an unnamed local variable to hold the list being iterated over
    Value* listVar = _context->createLocal(ValueType::Reference, "");
    _currentFunction->locals.push_back(listVar);

    emit(new JumpInst(loopInit));
    setBlock(loopInit);

    Value* initialList = visitAndGet(node->listExpression);
    emit(new StoreInst(listVar, initialList));

    emit(new JumpInst(loopBegin));
    setBlock(loopBegin);

    // Loop while list variable is not null
    Value* currentList = createTemp(ValueType::Reference);
    emit(new LoadInst(currentList, listVar));
    Value* isNull = createTemp(ValueType::U64);
    emit(new CallInst(isNull, emptyFunction, {currentList}));
    emit(new JumpIfInst(isNull, loopExit, loopBody));

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    _currentLoopExit = loopExit;

    setBlock(loopBody);

    // Assign the head of the list to the induction variable
    currentList = createTemp(ValueType::Reference);
    emit(new LoadInst(currentList, listVar));
    Value* currentHead = createTemp(getValueType(node->varType, _typeContext));
    emit(new CallInst(currentHead, headFunction, {currentList}));
    emit(new StoreInst(getValue(node->symbol), currentHead));

    // Pop the head off the current list
    Value* currentTail = createTemp(ValueType::Reference);
    emit(new CallInst(currentTail, tailFunction, {currentList}));
    emit(new StoreInst(listVar, currentTail));

    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(loopBegin));

    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(ForeverNode* node)
{
    BasicBlock* loopBody = createBlock();
    BasicBlock* loopExit = createBlock();

    emit(new JumpInst(loopBody));
    setBlock(loopBody);

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    _currentLoopExit = loopExit;

    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(loopBody));

    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(BreakNode* node)
{
    emit(new JumpInst(_currentLoopExit));
}

void TACCodeGen::visit(AssignNode* node)
{
    Value* value = visitAndGet(node->rhs);

    // TODO: Is there a less-hacky way of doing this?
    Symbol* symbol;
    if (VariableNode* lhs = dynamic_cast<VariableNode*>(node->lhs))
    {
        Value* dest = getValue(lhs->symbol);
        emit(new StoreInst(dest, value));
    }
    else if (NullaryNode* lhs = dynamic_cast<NullaryNode*>(node->lhs))
    {
        Value* dest = getValue(lhs->symbol);
        emit(new StoreInst(dest, value));
    }
    else if (MemberAccessNode* lhs = dynamic_cast<MemberAccessNode*>(node->lhs))
    {
        lhs->object->accept(this);

        Value* structure = lhs->object->value;

        std::vector<size_t> layout = getConstructorLayout(lhs->constructorSymbol, lhs, lhs->typeAssignment);
        emit(new IndexedStoreInst(structure, _context->createConstantInt(ValueType::U64, sizeof(SplObject) + 8 * layout[lhs->memberIndex]), value));
    }
    else
    {
        assert(false);
    }
}

void TACCodeGen::visit(VariableDefNode* node)
{
    Value* dest = getValue(node->symbol);

    if (!node->symbol)
    {
        visitAndGet(node->rhs);
    }
    else
    {
        Value* value = visitAndGet(node->rhs);
        emit(new StoreInst(dest, value));
    }
}

void TACCodeGen::visit(LetNode* node)
{
    Value* body = visitAndGet(node->body);
    ValueConstructor* constructor = node->valueConstructor;

    std::vector<size_t> layout = getConstructorLayout(node->constructorSymbol, node, node->typeAssignment);

    // Copy over each of the members of the constructor pattern
    for (size_t i = 0; i < node->symbols.size(); ++i)
    {
        const Symbol* member = node->symbols.at(i);
        if (member)
        {
            ValueType type = getValueType(member->type);

            Value* tmp = createTemp(type);
            emit(new IndexedLoadInst(tmp, body, sizeof(SplObject) + 8 * layout[i]));
            emit(new StoreInst(getValue(member), tmp));
        }
    }
}

void TACConditionalCodeGen::visit(FunctionCallNode* node)
{
    if (node->symbol->kind == kFunction && dynamic_cast<FunctionSymbol*>(node->symbol)->isBuiltin)
    {
        if (node->target == "not")
        {
            assert(node->arguments.size() == 1);
            visitCondition(*node->arguments[0], _falseBranch, _trueBranch);
            return;
        }
    }

    wrapper(node);
}

void TACCodeGen::visit(FunctionCallNode* node)
{
    std::vector<Value*> arguments;
    for (auto& i : node->arguments)
    {
        i->accept(this);
        arguments.push_back(i->value);
    }

    node->value = createTemp(getValueType(node->type));
    Value* result = node->value;

    if (node->symbol->kind == kFunction && dynamic_cast<FunctionSymbol*>(node->symbol)->isBuiltin)
    {
        if (node->target == "not")
        {
            assert(arguments.size() == 1);

            BasicBlock* trueBranch = createBlock();
            BasicBlock* falseBranch = createBlock();
            BasicBlock* continueAt = createBlock();

            emit(new JumpIfInst(arguments[0], trueBranch, falseBranch));

            setBlock(falseBranch);
            emit(new JumpInst(continueAt));

            setBlock(trueBranch);
            emit(new JumpInst(continueAt));

            setBlock(continueAt);
            PhiInst* phi = new PhiInst(node->value);
            phi->addSource(trueBranch, _context->False);
            phi->addSource(falseBranch, _context->True);
            emit(phi);
            return;

        }
        else
        {
            assert(false);
        }
    }

    if (node->target == "unsafeMakeArray") // TODO: Identify this better
    {
        assert(node->symbol->kind == kFunction);
        assert(dynamic_cast<FunctionSymbol*>(node->symbol)->isExternal);

        Type* arrayType = node->type;
        ConstructedType* constructedType = arrayType->get<ConstructedType>();
        assert(constructedType);
        assert(constructedType->name() == "Array");

        Type* internalType = constructedType->typeParameters()[0];

        // unsafeMakeArray has an implicit final argument that tells the function
        // whether the underlying type is boxed or unboxed
        if (getValueType(internalType) == ValueType::Reference)
        {
            arguments.push_back(_context->createConstantInt(ValueType::U64, BOXED_ARRAY_TAG));
        }
        else
        {
            arguments.push_back(_context->createConstantInt(ValueType::U64, UNBOXED_ARRAY_TAG));
        }

        // Fall through
    }

    if (node->symbol->kind == kFunction)
    {
        Value* fn = getFunctionValue(node->symbol, node, node->typeAssignment);
        CallInst* inst = new CallInst(result, fn, arguments);

        FunctionSymbol* functionSymbol = dynamic_cast<FunctionSymbol*>(node->symbol);
        inst->ccall = functionSymbol->isExternal;
        inst->regpass = inst->ccall;
        emit(inst);
    }
    else /* node->symbol->kind == kVariable */
    {
        // The variable represents a closure, so extract the actual function
        // address
        Value* functionAddress = createTemp(ValueType::NonHeapAddress);
        Value* closure = createTemp(ValueType::Reference);
        emit(new LoadInst(closure, getValue(node->symbol)));
        emit(new IndexedLoadInst(functionAddress, closure, sizeof(SplObject)));

        CallInst* inst = new CallInst(result, functionAddress, arguments);
        emit(inst);
    }
}

void TACCodeGen::visit(BinopNode* node)
{
    Value* lhs = visitAndGet(node->lhs);
    Value* rhs = visitAndGet(node->rhs);

    node->value = createTemp(getValueType(node->type));

    switch(node->op)
    {
    case BinopNode::kAdd:
        {
            emit(new BinaryOperationInst(node->value, lhs, BinaryOperation::ADD, rhs));
            break;
        }

    case BinopNode::kSub:
        {
            emit(new BinaryOperationInst(node->value, lhs, BinaryOperation::SUB, rhs));
            break;
        }

    case BinopNode::kMul:
        {
            emit(new BinaryOperationInst(node->value, lhs, BinaryOperation::MUL, rhs));
            break;
        }

    case BinopNode::kDiv:
        {
            emit(new BinaryOperationInst(node->value, lhs, BinaryOperation::DIV, rhs));
            break;
        }

    case BinopNode::kMod:
        {
            emit(new BinaryOperationInst(node->value, lhs, BinaryOperation::MOD, rhs));
            break;
        }
    }
}

void TACCodeGen::visit(MethodCallNode* node)
{
    std::vector<Value*> arguments;

    // Target object is implicitly the first argument
    node->object->accept(this);
    arguments.push_back(node->object->value);

    for (auto& i : node->arguments)
    {
        i->accept(this);
        arguments.push_back(i->value);
    }

    node->value = createTemp(getValueType(node->type));

    if (node->symbol->kind == kMethod)
    {
        Value* method = getFunctionValue(node->symbol, node, node->typeAssignment);
        emit(new CallInst(node->value, method, arguments));
    }
    else if (node->symbol->kind == kTraitMethod)
    {
        TraitMethodSymbol* symbol = dynamic_cast<TraitMethodSymbol*>(node->symbol);
        assert(symbol);

        Type* objectType = substitute(substitute(node->object->type, node->typeAssignment), _typeContext);
        assert(isConcrete(objectType));
        assert(isSubtype(objectType, symbol->trait));

        MethodSymbol* methodSymbol = _astContext->symbolTable()->resolveConcreteMethod(node->methodName, objectType);
        assert(methodSymbol);

        TypeAssignment assignment;
        Type* parentType = instantiate(methodSymbol->parentType, assignment);
        auto result = tryUnify(parentType, objectType);
        assert(result.first);

        Value* method = getFunctionValue(methodSymbol, node, assignment);
        emit(new CallInst(node->value, method, arguments));
    }
}

void TACCodeGen::visit(ReturnNode* node)
{
    Value* result = visitAndGet(node->expression);
    emit(new ReturnInst(result));
}

void TACCodeGen::visit(VariableNode* node)
{
    assert(node->symbol->kind == kVariable);
    node->value = createTemp(getValueType(node->symbol->type));
    emit(new LoadInst(node->value, getValue(node->symbol)));
}

void TACCodeGen::visit(MemberAccessNode* node)
{
    node->value = createTemp(getValueType(node->type));

    node->object->accept(this);

    std::vector<size_t> layout = getConstructorLayout(node->constructorSymbol, node, node->typeAssignment);

    Value* structure = node->object->value;
    emit(new IndexedLoadInst(node->value, structure, sizeof(SplObject) + 8 * layout[node->memberIndex]));
}

void TACCodeGen::visit(DataDeclaration* node)
{
    for (ConstructorSymbol* symbol : node->constructorSymbols)
    {
        _constructors.push_back(symbol);
    }
}

void TACCodeGen::visit(StructDefNode* node)
{
    _constructors.push_back(node->constructorSymbol);
}

void TACCodeGen::visit(MatchNode* node)
{
    std::vector<BasicBlock*> caseLabels;
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        caseLabels.push_back(createBlock());
    }
    BasicBlock* continueAt = createBlock();

    Value* expr = visitAndGet(node->expr);

    // TODO: Handle case where all constructors are parameter-less

    Value* tag = createTemp(ValueType::U64);
    emit(new IndexedLoadInst(tag, expr, offsetof(SplObject, constructorTag)));

    // Jump to the appropriate case based on the tag
    BasicBlock* nextTest = _currentBlock;
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        auto& arm = node->arms[i];
        size_t armTag = arm->constructorTag;

        setBlock(nextTest);
        nextTest = createBlock();
        emit(new ConditionalJumpInst(tag, "==", _context->createConstantInt(ValueType::U64, armTag), caseLabels[i], nextTest));
    }

    // Match must be exhaustive, so we should never fail all tests
    setBlock(nextTest);
    emit(new UnreachableInst);

    // Individual arms
    Value* lastSwitchExpr = _currentSwitchExpr;
    _currentSwitchExpr = expr;
    bool canReach = false;
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        auto& arm = node->arms[i];

        setBlock(caseLabels[i]);
        arm->accept(this);

        if (!_currentBlock->isTerminated())
        {
            canReach = true;
            emit(new JumpInst(continueAt));
        }
    }

    _currentSwitchExpr = lastSwitchExpr;

    setBlock(continueAt);
    if (!canReach)
    {
        emit(new UnreachableInst);
    }
}

void TACCodeGen::visit(MatchArm* node)
{
    ValueConstructor* constructor = node->valueConstructor;

    std::vector<size_t> layout = getConstructorLayout(node->constructorSymbol, node, node->typeAssignment);

    // Copy over each of the members of the constructor pattern
    for (size_t i = 0; i < node->symbols.size(); ++i)
    {
        const Symbol* member = node->symbols.at(i);
        if (member)
        {
            Value* tmp = createTemp(getValueType(member->type));
            emit(new IndexedLoadInst(tmp, _currentSwitchExpr, sizeof(SplObject) + 8 * layout[i]));
            emit(new StoreInst(getValue(member), tmp));
        }
    }

    node->body->accept(this);
}

void TACCodeGen::visit(StringLiteralNode* node)
{
    node->value = getValue(node->symbol);
}

void TACCodeGen::createConstructor(const ConstructorSymbol* symbol, const TypeAssignment& typeAssignment)
{
    ValueConstructor* constructor = symbol->constructor;
    const std::vector<ValueConstructor::MemberDesc> members = constructor->members();
    size_t constructorTag = constructor->constructorTag();

    Value* result = createTemp(ValueType::Reference);

    // For now, every member takes up exactly 8 bytes (either directly or as a pointer).
    size_t size = sizeof(SplObject) + 8 * members.size();

    // Allocate room for the object
    CallInst* inst = new CallInst(
        result,
        _context->createExternFunction("gcAllocate"), // TODO: Fix this
        {_context->createConstantInt(ValueType::U64, size)});
    inst->regpass = true;
    emit(inst);

    //// Fill in the members with the constructor arguments

    // SplObject header fields
    std::vector<size_t> layout = getConstructorLayout(symbol, nullptr, typeAssignment);
    size_t numReferences = getNumPointers(symbol, nullptr, typeAssignment);

    emit(new IndexedStoreInst(result, _context->createConstantInt(ValueType::U64, offsetof(SplObject, constructorTag)), _context->createConstantInt(ValueType::U64, constructorTag)));
    emit(new IndexedStoreInst(result, _context->createConstantInt(ValueType::U64, offsetof(SplObject, numReferences)), _context->createConstantInt(ValueType::U64, numReferences)));

    // Individual members
    for (size_t i = 0; i < members.size(); ++i)
    {
        auto& member = members[i];
        size_t location = layout[i];

        std::string name = member.name;
        if (name.empty()) name = std::to_string(i);

        Value* param = _context->createArgument(getValueType(member.type, typeAssignment), name);
        _currentFunction->params.push_back(param);

        Value* temp = createTemp(getValueType(substitute(member.type, typeAssignment)));
        emit(new LoadInst(temp, param));
        emit(new IndexedStoreInst(result, _context->createConstantInt(ValueType::U64, sizeof(SplObject) + 8 * location), temp));
    }

    emit(new ReturnInst(result));
}

void TACCodeGen::visit(ImplNode* node)
{
    AstVisitor::visit(node);
}
