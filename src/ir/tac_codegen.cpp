#include "ir/tac_codegen.hpp"

#include "ast/ast_context.hpp"
#include "lib/library.h"
#include "semantic/types.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>

TACCodeGen::TACCodeGen(TACContext* context)
: _context(context), _conditionalCodeGen(this)
{
}

void TACCodeGen::codeGen(AstContext* astContext)
{
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
        std::string name = baseType->name();
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
        std::string name = constructedType->typeConstructor()->name();
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

static ValueType getValueType(Type* type)
{
    if (type->isBoxed())
    {
        return ValueType::ReferenceType;
    }
    else
    {
        return ValueType::Integer;
    }
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

static bool sameAssignment(const TypeAssignment& lhs, const TypeAssignment& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (auto& assignment : lhs)
    {
        auto match = rhs.find(assignment.first);
        if (match == rhs.end())
            return false;

        if (!isCompatible(assignment.second, match->second))
            return false;
    }

    return true;
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
                return typeTable->createConstructedType(constructedType->typeConstructor(), newParams);
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

size_t TACCodeGen::getNumPointers(const ConstructorSymbol* symbol, AstNode* node, const TypeAssignment& typeAssignment)
{
    // Perform the layout
    getConstructorLayout(symbol, node, typeAssignment);

    Function* function = getFunctionValue(symbol, node, typeAssignment);
    return _constructorLayouts.at(function).first;
}

std::vector<size_t> TACCodeGen::getConstructorLayout(const ConstructorSymbol* symbol, AstNode* node, const TypeAssignment& typeAssignment)
{
    Function* function = getFunctionValue(symbol, node, typeAssignment);
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

Function* TACCodeGen::getFunctionValue(const Symbol* symbol, AstNode* node, const TypeAssignment& typeAssignment)
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
            if (!instantiations.empty())
            {
                return instantiations[0].second;
            }
            else
            {
                result = _context->createExternFunction(symbol->name);
                TypeAssignment empty;
                instantiations.emplace_back(empty, result);
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
            ss << baseType->name();
        }
        else if (ConstructedType* constructedType = methodSymbol->parentType->get<ConstructedType>())
        {
            ss << constructedType->typeConstructor()->name();
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

    emit(new ReturnInst());

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
            Function* function = getFunctionValue(funcDefNode->symbol, nullptr, typeContext);

            _currentFunction = function;
            _localNames.clear();
            setBlock(createBlock());

            // Collect all function parameters
            for (Symbol* param : funcDefNode->parameterSymbols)
            {
                assert(dynamic_cast<VariableSymbol*>(param)->isParam);
                _currentFunction->params.push_back(getValue(param));
            }

            // Generate code for the function body
            _typeContext = typeContext;
            funcDefNode->body->accept(this);

            // Handle implicit return values
            if (!_currentBlock->isTerminated())
            {
                emit(new ReturnInst(funcDefNode->body->value));
            }
        }
        else
        {
            const FunctionSymbol* functionSymbol = dynamic_cast<const FunctionSymbol*>(symbol);
            assert(functionSymbol);

            if (functionSymbol->isConstructor)
            {
                const ConstructorSymbol* constructorSymbol = dynamic_cast<const ConstructorSymbol*>(symbol);
                assert(constructorSymbol);

                Function* function = getFunctionValue(constructorSymbol, nullptr, typeContext);
                _currentFunction = function;
                setBlock(createBlock());

                _typeContext.clear();
                createConstructor(constructorSymbol, typeContext);
            }
            else
            {
                assert(functionSymbol->isBuiltin);

                Function* function = getFunctionValue(functionSymbol, nullptr, typeContext);

                _currentFunction = function;
                _localNames.clear();
                _typeContext = typeContext;
                setBlock(createBlock());

                if (functionSymbol->name == "makeArray")
                {
                    builtin_makeArray(functionSymbol, typeContext);
                }
                else
                {
                    std::cerr << functionSymbol->name << std::endl;
                    assert(false);
                }
            }
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
    node->value = createTemp(ValueType::Integer);
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
    node->value = createTemp(ValueType::Integer);
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
        ValueType type = getValueType(node->type);
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
            CallInst* callInst = new CallInst(dest, _context->createExternFunction("gcAllocate"), {_context->getConstantInt(size)});
            callInst->regpass = true;
            emit(callInst);

            // SplObject header fields
            emit(new IndexedStoreInst(dest, _context->getConstantInt(offsetof(SplObject, constructorTag)), _context->Zero));
            emit(new IndexedStoreInst(dest, _context->getConstantInt(offsetof(SplObject, numPointers)), _context->Zero));

            // Address of the function as an unboxed member
            Value* fn = getFunctionValue(node->symbol, node, node->typeAssignment);
            emit(new IndexedStoreInst(dest, _context->getConstantInt(sizeof(SplObject)), fn));
        }
    }
}

void TACCodeGen::visit(IntNode* node)
{
    node->value = _context->getConstantInt(node->intValue);
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
    Value* dieFunction = getFunctionValue(node->dieSymbol, node);

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
    CallInst* inst = new CallInst(nullptr, dieFunction, {message});
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
    Value* listVar = _context->createLocal(ValueType::ReferenceType, "");
    _currentFunction->locals.push_back(listVar);

    emit(new JumpInst(loopInit));
    setBlock(loopInit);

    Value* initialList = visitAndGet(node->listExpression);
    emit(new StoreInst(listVar, initialList));

    emit(new JumpInst(loopBegin));
    setBlock(loopBegin);

    // Loop while list variable is not null
    Value* currentList = createTemp(ValueType::ReferenceType);
    emit(new LoadInst(currentList, listVar));
    Value* isNull = createTemp(ValueType::Integer);
    emit(new CallInst(isNull, emptyFunction, {currentList}));
    emit(new JumpIfInst(isNull, loopExit, loopBody));

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    _currentLoopExit = loopExit;

    setBlock(loopBody);

    // Assign the head of the list to the induction variable
    currentList = createTemp(ValueType::ReferenceType);
    emit(new LoadInst(currentList, listVar));
    Value* currentHead = createTemp(getValueType(substitute(node->varType, _typeContext)));
    emit(new CallInst(currentHead, headFunction, {currentList}));
    emit(new StoreInst(getValue(node->symbol), currentHead));

    // Pop the head off the current list
    Value* currentTail = createTemp(ValueType::ReferenceType);
    emit(new CallInst(currentTail, tailFunction, {currentList}));
    emit(new StoreInst(listVar, currentTail));

    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(loopBegin));

    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(ForNode* node)
{
    BasicBlock* loopInit = createBlock();
    BasicBlock* loopBegin = createBlock();
    BasicBlock* loopExit = createBlock();
    BasicBlock* loopBody = createBlock();

    Value* inductionVar = getValue(node->symbol);

    emit(new JumpInst(loopInit));
    setBlock(loopInit);

    // Compute the "from" and "to" values, and initialize the induction variable
    // to "from"
    Value* from = visitAndGet(node->fromExpression);
    Value* to = visitAndGet(node->toExpression);
    emit(new StoreInst(inductionVar, from));

    emit(new JumpInst(loopBegin));
    setBlock(loopBegin);

    // Loop while current <= to
    Value* current = createTemp(ValueType::Integer);
    emit(new LoadInst(current, inductionVar));
    emit(new ConditionalJumpInst(current, ">", to, loopExit, loopBody));

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    _currentLoopExit = loopExit;

    setBlock(loopBody);

    node->body->accept(this);

    if (!_currentBlock->isTerminated())
    {
        // Increment the induction variable
        Value* current = createTemp(ValueType::Integer);
        emit(new LoadInst(current, inductionVar));
        Value* next = createTemp(ValueType::Integer);
        emit(new BinaryOperationInst(next, current, BinaryOperation::ADD, _context->One));
        emit(new StoreInst(inductionVar, next));

        emit(new JumpInst(loopBegin));
    }

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
        emit(new IndexedStoreInst(structure, _context->getConstantInt(sizeof(SplObject) + 8 * layout[lhs->memberIndex]), value));
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

    if (!node->type->equals(node->type->table()->Unit))
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
        else if (node->target == "+")
        {
            assert(arguments.size() == 2);
            emit(new BinaryOperationInst(result, arguments[0], BinaryOperation::ADD, arguments[1]));
            return;

        }
        else if (node->target == "-")
        {
            assert(arguments.size() == 2);
            emit(new BinaryOperationInst(result, arguments[0], BinaryOperation::SUB, arguments[1]));
            return;
        }
        else if (node->target == "*")
        {
            assert(arguments.size() == 2);
            emit(new BinaryOperationInst(result, arguments[0], BinaryOperation::MUL, arguments[1]));
            return;
        }
        else if (node->target == "/")
        {
            assert(arguments.size() == 2);
            emit(new BinaryOperationInst(result, arguments[0], BinaryOperation::DIV, arguments[1]));
            return;
        }
        else if (node->target == "%")
        {
            assert(arguments.size() == 2);
            emit(new BinaryOperationInst(result, arguments[0], BinaryOperation::MOD, arguments[1]));
            return;
        }
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
        Value* functionAddress = createTemp(ValueType::CodeAddress);
        Value* closure = createTemp(ValueType::ReferenceType);
        emit(new LoadInst(closure, getValue(node->symbol)));
        emit(new IndexedLoadInst(functionAddress, closure, sizeof(SplObject)));

        CallInst* inst = new CallInst(result, functionAddress, arguments);
        emit(inst);
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

    if (!node->type->equals(node->type->table()->Unit))
        node->value = createTemp(getValueType(node->type));

    Value* result = node->value;

    assert(node->symbol->kind == kMethod);

    Value* method = getFunctionValue(node->symbol, node, node->typeAssignment);
    emit(new CallInst(result, method, arguments));
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

    // Get constructor tag of expr (either as a immediate, or from the object
    // header)
    BasicBlock* gotTag = createBlock();
    BasicBlock* isImmediate = createBlock();
    BasicBlock* isObject = createBlock();

    // TODO: Handle case where all constructors are parameter-less

    // Check for immediate, and remove tag bit
    Value* checked = createTemp(ValueType::Integer);
    emit(new BinaryOperationInst(checked, expr, BinaryOperation::AND, _context->One));
    emit(new ConditionalJumpInst(checked, "==", _context->Zero, isObject, isImmediate));

    setBlock(isImmediate);
    Value* tag1 = createTemp(ValueType::Integer);
    emit(new BinaryOperationInst(tag1, expr, BinaryOperation::SHR, _context->One));
    emit(new JumpInst(gotTag));

    // Otherwise, extract tag from object header
    setBlock(isObject);
    Value* tag2 = createTemp(ValueType::Integer);
    emit(new IndexedLoadInst(tag2, expr, offsetof(SplObject, constructorTag)));
    emit(new JumpInst(gotTag));

    setBlock(gotTag);
    Value* tag = createTemp(ValueType::Integer);
    PhiInst* phi = new PhiInst(tag);
    phi->addSource(isImmediate, tag1);
    phi->addSource(isObject, tag2);
    emit(phi);

    // Jump to the appropriate case based on the tag
    BasicBlock* nextTest = _currentBlock;
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        auto& arm = node->arms[i];
        size_t armTag = arm->constructorTag;

        setBlock(nextTest);
        nextTest = createBlock();
        emit(new ConditionalJumpInst(tag, "==", _context->getConstantInt(armTag), caseLabels[i], nextTest));
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

void TACCodeGen::builtin_makeArray(const FunctionSymbol* symbol, const TypeAssignment& typeAssignment)
{
    FunctionType* functionType = symbol->type->get<FunctionType>();
    assert(functionType);
    assert(functionType->inputs().size() == 2);

    // foreign makeArray<T>(n: Int, value: T) -> Array<T>
    Value* param_n = _context->createArgument(getValueType(substitute(functionType->inputs()[0], typeAssignment)), "n");
    _currentFunction->params.push_back(param_n);

    Value* param_value = _context->createArgument(getValueType(substitute(functionType->inputs()[1], typeAssignment)), "value");
    _currentFunction->params.push_back(param_value);

    Value* size = createTemp(ValueType::Integer);
    emit(new LoadInst(size, param_n));

    Value* value = createTemp(param_value->type);
    emit(new LoadInst(value, param_value));

    // TODO: Check for negative size

    // Allocate room for the object (allocSize = sizeof(SplObject) + 8 * n)
    Value* tmp = createTemp(ValueType::Integer);
    Value* allocSize = createTemp(ValueType::Integer);
    emit(new BinaryOperationInst(tmp, size, BinaryOperation::MUL, _context->getConstantInt(8)));
    emit(new BinaryOperationInst(allocSize, tmp, BinaryOperation::ADD, _context->getConstantInt(sizeof(SplObject))));

    Value* result = createTemp(ValueType::ReferenceType);
    CallInst* inst = new CallInst(
        result,
        _context->createExternFunction("gcAllocate"), // TODO: Fix this
        {allocSize});
    inst->regpass = true;
    emit(inst);

    ConstructedType* arrayType = functionType->output()->get<ConstructedType>();
    assert(arrayType);
    assert(arrayType->typeParameters().size() == 1);

    // For the type Array<T>, determine if T is boxed or unboxed
    Type* underlyingType = substitute(arrayType->typeParameters()[0], typeAssignment);
    Value* numPointers;
    if (underlyingType->isBoxed())
    {
        numPointers = size;
    }
    else
    {
        numPointers = _context->Zero;
    }

    emit(new IndexedStoreInst(result, _context->getConstantInt(offsetof(SplObject, constructorTag)), _context->Zero));
    emit(new IndexedStoreInst(result, _context->getConstantInt(offsetof(SplObject, numPointers)), numPointers));

    // Get a pointer to the actual array data, just past the SplObject header
    Value* arrayContent = createTemp(ValueType::Integer);
    emit(new BinaryOperationInst(arrayContent, result, BinaryOperation::ADD, _context->getConstantInt(2 * 8)));

    Value* counter = _context->createLocal(ValueType::Integer, "i");
    _currentFunction->locals.push_back(counter);

    BasicBlock* startLoop = createBlock();
    BasicBlock* loopBody = createBlock();
    BasicBlock* endLoop = createBlock();

    // i := 0
    emit(new StoreInst(counter, _context->Zero));
    emit(new JumpInst(startLoop));


    // if !(i < size) break
    setBlock(startLoop);
    Value* i = createTemp(ValueType::Integer);
    emit(new LoadInst(i, counter));
    emit(new ConditionalJumpInst(i, "<", size, loopBody, endLoop));

    // result[i] = value
    setBlock(loopBody);
    Value* tmp2 = createTemp(ValueType::Integer);
    Value* offset = createTemp(ValueType::Integer);
    emit(new BinaryOperationInst(tmp2, i, BinaryOperation::MUL, _context->getConstantInt(8)));
    emit(new BinaryOperationInst(offset, tmp2, BinaryOperation::ADD, _context->getConstantInt(2 * 8)));
    emit(new IndexedStoreInst(result, offset, value));

    // i += 1
    Value* currentCounter = createTemp(ValueType::Integer);
    Value* nextCounter = createTemp(ValueType::Integer);
    emit(new LoadInst(currentCounter, counter));
    emit(new BinaryOperationInst(nextCounter, currentCounter, BinaryOperation::ADD, _context->One));
    emit(new StoreInst(counter, nextCounter));

    emit(new JumpInst(startLoop));


    setBlock(endLoop);
    emit(new ReturnInst(result));
}

void TACCodeGen::createConstructor(const ConstructorSymbol* symbol, const TypeAssignment& typeAssignment)
{
    ValueConstructor* constructor = symbol->constructor;
    const std::vector<ValueConstructor::MemberDesc> members = constructor->members();
    size_t constructorTag = constructor->constructorTag();

    Value* result = createTemp(ValueType::ReferenceType);

    // For now, every member takes up exactly 8 bytes (either directly or as a pointer).
    size_t size = sizeof(SplObject) + 8 * members.size();

    // Allocate room for the object
    CallInst* inst = new CallInst(
        result,
        _context->createExternFunction("gcAllocate"), // TODO: Fix this
        {_context->getConstantInt(size)});
    inst->regpass = true;
    emit(inst);

    //// Fill in the members with the constructor arguments

    // SplObject header fields
    std::vector<size_t> layout = getConstructorLayout(symbol, nullptr, typeAssignment);
    size_t numPointers = getNumPointers(symbol, nullptr, typeAssignment);

    emit(new IndexedStoreInst(result, _context->getConstantInt(offsetof(SplObject, constructorTag)), _context->getConstantInt(constructorTag)));
    emit(new IndexedStoreInst(result, _context->getConstantInt(offsetof(SplObject, numPointers)), _context->getConstantInt(numPointers)));

    // Individual members
    for (size_t i = 0; i < members.size(); ++i)
    {
        auto& member = members[i];
        size_t location = layout[i];

        std::string name = member.name;
        if (name.empty()) name = std::to_string(i);

        Value* param = _context->createArgument(getValueType(substitute(member.type, typeAssignment)), name);
        _currentFunction->params.push_back(param);

        Value* temp = createTemp(getValueType(member.type));
        emit(new LoadInst(temp, param));
        emit(new IndexedStoreInst(result, _context->getConstantInt(sizeof(SplObject) + 8 * location), temp));
    }

    emit(new ReturnInst(result));
}

void TACCodeGen::visit(ImplNode* node)
{
    AstVisitor::visit(node);
}
