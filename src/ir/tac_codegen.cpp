#include "ir/tac_codegen.hpp"

#include "ast/ast_context.hpp"
#include "lib/library.h"
#include "semantic/subtype.hpp"
#include "semantic/type_functions.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <iostream>

TACCodeGen::TACCodeGen(TACContext* context)
: _context(context), _conditionalCodeGen(this)
{
    _gcAllocate = _context->createExternFunction("gcAllocate");
}

void TACCodeGen::codeGen(AstContext* astContext)
{
    _astContext = astContext;
    _astContext->root()->accept(this);
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
        std::string name = type->str();
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

Value* TACCodeGen::load(const Symbol* symbol)
{
    assert(symbol);

    Value* dest = createTemp(getValueType(symbol->type));

    if (symbol->kind == kCapture)
    {
        const CaptureSymbol* captureSymbol = dynamic_cast<const CaptureSymbol*>(symbol);
        assert(captureSymbol);

        Value* env = load(captureSymbol->envSymbol);
        emit(new IndexedLoadInst(dest, env, constant(sizeof(SplObject) + 8 * captureSymbol->index)));
    }
    else
    {
        Value* src = getValue(symbol);
        emit(new LoadInst(dest, src));
    }

    return dest;
}

void TACCodeGen::store(const Symbol* symbol, Value* src)
{
    assert(symbol);

    if (symbol->kind == kCapture)
    {
        const CaptureSymbol* captureSymbol = dynamic_cast<const CaptureSymbol*>(symbol);
        assert(captureSymbol);

        Value* env = load(captureSymbol->envSymbol);
        emit(new IndexedStoreInst(env, constant(sizeof(SplObject) + 8 * captureSymbol->index), src));
    }
    else
    {
        Value* dest = getValue(symbol);
        emit(new StoreInst(dest, src));
    }
}

Value* TACCodeGen::getValue(const Symbol* symbol)
{
    if (!symbol)
    {
        return nullptr;
    }

    auto i = _globalNames.find(symbol);
    if (i != _globalNames.end())
    {
        return i->second;
    }

    auto j = _localNames.find(symbol);
    if (j != _localNames.end())
    {
        return j->second;
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
        {
            // Any leftover 'T: Num variables are assumed to be Int, and
            // any other unquantified variables becomes Unit (if all other
            // constraints match)
            TypeVariable* var = original->get<TypeVariable>();

            if (!var->quantified())
            {
                bool numFound = false;
                for (Trait* constraint : var->constraints())
                {
                    if (constraint->prototype() == var->table()->Num)
                    {
                        numFound = true;
                        break;
                    }
                }

                if (numFound)
                {
                    for (Trait* constraint : var->constraints())
                    {
                        if (!isSubtype(var->table()->Int, constraint))
                        {
                            return false;
                        }
                    }

                    var->assign(var->table()->Int);
                }
                else
                {
                    for (Trait* constraint : var->constraints())
                    {
                        if (!isSubtype(var->table()->Unit, constraint))
                        {
                            return false;
                        }
                    }

                    var->assign(var->table()->Unit);
                }

                return true;
            }

            return false;
        }

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

static TypeAssignment combine(const TypeAssignment& typeContext, const TypeAssignment& typeAssignment)
{
    TypeAssignment result = typeAssignment;

    for (auto& assignment : typeAssignment)
    {
        result[assignment.first] = substitute(assignment.second, typeContext);
    }

    return result;
}

static TypeAssignment compose(const TypeAssignment& typeContext, const TypeAssignment& typeAssignment)
{
    TypeAssignment result = typeContext;

    for (auto& assignment : typeAssignment)
    {
        result[assignment.first] = substitute(assignment.second, typeContext);
    }

    return result;
}

static ValueType getRealValueType(Type* type)
{
    if (!isConcrete(type))
    {
        throw MonomorphizationError();
    }

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

Type* TACCodeGen::getConcreteType(Type* type, const TypeAssignment& typeAssignment)
{
    TypeAssignment fullAssignment = compose(_typeContext, typeAssignment);
    Type* concreteType = substitute(type, fullAssignment);

    return concreteType;
}

ValueType TACCodeGen::getValueType(Type* type, const TypeAssignment& typeAssignment)
{
    return getRealValueType(getConcreteType(type, typeAssignment));
}

uint64_t TACCodeGen::getConstructorLayout(const ConstructorSymbol* symbol, AstNode* node, const TypeAssignment& typeAssignment)
{
    Function* function = (Function*)getFunctionValue(symbol, node, typeAssignment);
    auto i = _constructorLayouts.find(function);
    if (i != _constructorLayouts.end())
    {
        return i->second;
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

    if (members.size() > 64)
    {
        YYLTYPE location = node->location;
        std::stringstream ss;

        ss << location.filename << ":" << location.first_line << ":" << location.first_column
           << ": constructor `" << symbol->name << "` cannot contain more than 64 members";

        throw CodegenError(ss.str());
    }

    uint64_t refMask = 0;
    for(size_t i = 0; i < members.size(); ++i)
    {
        auto& member = members[i];

        Type* type = substitute(member.type, realAssignment);
        assert(isConcrete(type));

        if (type->isBoxed())
        {
            refMask |= (1 << i);
        }
    }

    _constructorLayouts.emplace(function, refMask);
    return refMask;
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
            ss << methodSymbol->parentType->str();
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
                    // TODO: This may be ambiguous
                    std::string name = constraint->name();
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

Value* TACCodeGen::getTraitMethodValue(Type* objectType, const Symbol* symbol, AstNode* node, const TypeAssignment& typeAssignment)
{
    assert(symbol->kind == kTraitMethod);
    TraitSymbol* traitSymbol = dynamic_cast<const TraitMethodSymbol*>(symbol)->traitSymbol;
    Trait* trait = traitSymbol->trait;

    objectType = substitute(substitute(objectType, typeAssignment), _typeContext);
    assert(isConcrete(objectType));
    assert(isSubtype(objectType, trait));

    MethodSymbol* methodSymbol = _astContext->symbolTable()->resolveTraitInstanceMethod(symbol->name, objectType, traitSymbol);
    assert(methodSymbol);

    TypeAssignment assignment;
    Type* parentType = instantiate(methodSymbol->parentType, assignment);
    auto result = tryUnify(parentType, objectType);
    assert(result.first);

    return getFunctionValue(methodSymbol, node, assignment);
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
    Function* main = _context->createFunction("encmain");
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
            const FunctionSymbol* functionSymbol = dynamic_cast<const FunctionSymbol*>(symbol);

            if (functionSymbol->isBuiltin)
            {
                Function* function = (Function*)getFunctionValue(functionSymbol, nullptr, typeContext);

                TypeAssignment realAssignment = combine(_typeContext, typeContext);
                Type* functionType = substitute(functionSymbol->type, realAssignment);

                _currentFunction = function;
                _localNames.clear();
                _typeContext = typeContext;
                setBlock(createBlock());

                // TODO: Make this into some kind of map
                if (symbol->name == "unsafeEmptyArray")
                {
                    makeArray(functionType, false);
                }
                else if (symbol->name == "unsafeZeroArray")
                {
                    makeArray(functionType, true);
                }
                else
                {
                    assert(false);
                }
            }
            else if (functionSymbol->isConstructor)
            {
                const ConstructorSymbol* constructorSymbol = dynamic_cast<const ConstructorSymbol*>(symbol);
                assert(constructorSymbol);

                Function* function = (Function*)getFunctionValue(constructorSymbol, nullptr, typeContext);
                _currentFunction = function;
                setBlock(createBlock());

                _typeContext.clear();
                createConstructor(constructorSymbol, typeContext);
            }
            else if (functionSymbol->isLambda)
            {
                Function* function = (Function*)getFunctionValue(functionSymbol, nullptr, typeContext);

                _currentFunction = function;
                _localNames.clear();
                _typeContext = typeContext;
                setBlock(createBlock());

                LambdaNode* lambdaNode = dynamic_cast<LambdaNode*>(symbol->node);
                assert(lambdaNode);

                _currentFunction->params.push_back(getValue(lambdaNode->paramSymbol));
                Value* env = getValue(lambdaNode->envSymbol);
                _currentFunction->params.push_back(env);

                lambdaNode->body->accept(this);
                emit(new ReturnInst(lambdaNode->body->value));
            }
            else
            {
                assert(false);
            }
        }
    }
}

void TACConditionalCodeGen::visit(ComparisonNode* node)
{
    Value* lhs = visitAndGet(node->lhs);
    Value* rhs = visitAndGet(node->rhs);

    if (node->method)
    {
        Value* method = _mainCodeGen->getTraitMethodValue(node->lhs->type, node->method, node);
        Value* condition = _mainCodeGen->createTemp(ValueType::U64);
        emit(new CallInst(condition, method, {lhs, rhs}));
        emit(new JumpIfInst(condition, _trueBranch, _falseBranch));
        return;
    }

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
    node->value = createTemp(ValueType::U64);

    if (node->method)
    {
        Value* method = getTraitMethodValue(node->lhs->type, node->method, node);
        emit(new CallInst(node->value, method, {lhs, rhs}));
        return;
    }

    BasicBlock* trueBranch = createBlock();
    BasicBlock* falseBranch = createBlock();
    BasicBlock* continueAt = createBlock();

    emit(new ConditionalJumpInst(lhs, operation, rhs, trueBranch, falseBranch));

    setBlock(falseBranch);
    emit(new JumpInst(continueAt));

    setBlock(trueBranch);
    emit(new JumpInst(continueAt));

    setBlock(continueAt);
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
        node->value = load(node->symbol);
    }
    else
    {
        FunctionSymbol* functionSymbol = dynamic_cast<FunctionSymbol*>(node->symbol);

        Value* dest = node->value = createTemp();

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

            Value* fn = getFunctionValue(node->symbol, node, node->typeAssignment);
            createClosure(dest, fn);
        }

        dest->type = getValueType(node->type, node->typeAssignment);
    }
}

void TACCodeGen::createClosure(Value* dest, Value* fn, const std::vector<Symbol*>& captures)
{
    Value* env;
    if (!captures.empty())
    {
        env = createTemp(ValueType::Reference);

        gcAllocate(env, sizeof(SplObject) + 8 * captures.size());

        // SplObject header fields
        emit(new IndexedStoreInst(env, constant(offsetof(SplObject, constructorTag)), _context->Zero));

        uint64_t refMask = 0;
        for (size_t i = 0; i < captures.size(); ++i)
        {
            Type* captureType = substitute(captures[i]->type, _typeContext);
            if (captureType->isBoxed())
            {
                refMask |= (1 << i);
            }
        }

        emit(new IndexedStoreInst(env, constant(offsetof(SplObject, refMask)), constant(refMask)));

        for (size_t i = 0; i < captures.size(); ++i)
        {
            Symbol* symbol = captures[i];

            Value* temp = load(symbol);
            emit(new IndexedStoreInst(env, constant(sizeof(SplObject) + 8 * i), temp));
        }
    }
    else
    {
        env = constant(0);
    }

    // Closure format:
    // (offset 0) function address
    // (offset 8) pointer to environment
    gcAllocate(dest, sizeof(SplObject) + 16);

    // SplObject header fields
    emit(new IndexedStoreInst(dest, constant(offsetof(SplObject, constructorTag)), _context->Zero));
    emit(new IndexedStoreInst(dest, constant(offsetof(SplObject, refMask)), constant(2)));
    emit(new IndexedStoreInst(dest, constant(sizeof(SplObject)), fn));
    emit(new IndexedStoreInst(dest, constant(sizeof(SplObject) + 8), env));
}

static void getTrivialAssignment(Type* type, TypeAssignment& result)
{
    switch (type->tag())
    {
        case ttBase:
            return;

        case ttVariable:
        {
            TypeVariable* typeVariable = type->get<TypeVariable>();
            if (typeVariable->quantified())
            {
                result[typeVariable] = type;
            }

            return;
        }

        case ttFunction:
        {
            FunctionType* functionType = type->get<FunctionType>();

            for (auto& input : functionType->inputs())
            {
                getTrivialAssignment(input, result);
            }

            getTrivialAssignment(functionType->output(), result);

            return;
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = type->get<ConstructedType>();
            for (Type* parameter : constructedType->typeParameters())
            {
                getTrivialAssignment(parameter, result);
            }

            return;
        }
    }
}

void TACCodeGen::visit(LambdaNode* node)
{
    TypeAssignment trivialAssignment;
    getTrivialAssignment(node->type, trivialAssignment);

    node->value = createTemp();
    Value* fn = getFunctionValue(node->functionSymbol, node, trivialAssignment);
    createClosure(node->value, fn, node->captures);
}

static bool checkIntRange(ValueType type, int64_t value)
{
    if (isSigned(type))
    {
        int64_t lower, upper;
        switch (getSize(type))
        {
            case 64:
                lower = std::numeric_limits<int64_t>::min();
                upper = std::numeric_limits<int64_t>::max();
                break;

            case 32:
                lower = std::numeric_limits<int32_t>::min();
                upper = std::numeric_limits<int32_t>::max();
                break;

            case 16:
                lower = std::numeric_limits<int16_t>::min();
                upper = std::numeric_limits<int16_t>::max();
                break;

            case 8:
                lower = std::numeric_limits<int8_t>::min();
                upper = std::numeric_limits<int8_t>::max();
                break;

            default:
                assert(false);
        }

        return (lower <= value && value <= upper);
    }
    else
    {
        uint64_t uvalue = static_cast<uint64_t>(value);

        uint64_t upper;
        switch (getSize(type))
        {
            case 64:
                upper = std::numeric_limits<uint64_t>::max();
                break;

            case 32:
                upper = std::numeric_limits<uint32_t>::max();
                break;

            case 16:
                upper = std::numeric_limits<uint16_t>::max();
                break;

            case 8:
                upper = std::numeric_limits<uint8_t>::max();
                break;

            default:
                assert(false);
        }

        return uvalue <= upper;
    }
}

void TACCodeGen::visit(IntNode* node)
{
    try
    {
        ValueType type = getValueType(node->type);
        assert(isInteger(type));

        if (node->suffix.empty())
        {
            if ((node->negative && !isSigned(type)) || !checkIntRange(type, node->intValue))
            {
                YYLTYPE location = node->location;
                std::stringstream ss;

                ss << location.filename << ":" << location.first_line << ":" << location.first_column
                    << ": integer literal is out of range for inferred type " << node->type->str();

                throw CodegenError(ss.str());
            }
        }

        node->value = _context->createConstantInt(type, node->intValue);
    }
    catch (MonomorphizationError& e)
    {
        // This can happen if the result accumulates some additional constraints
        // not satisfied by Int. We could try harder in this case to find a type
        // that works.

        YYLTYPE location = node->location;
        std::stringstream ss;

        ss << location.filename << ":" << location.first_line << ":" << location.first_column
            << ": cannot infer concrete type for integer (try adding a suffix)";

        throw CodegenError(ss.str());
    }
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

void TACCodeGen::visit(IfElseNode* node)
{
    BasicBlock* trueBranch = createBlock();
    BasicBlock* falseBranch = createBlock();

    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, falseBranch);

    BasicBlock* continueAt = nullptr;

    setBlock(trueBranch);
    node->body->accept(this);
    if (!_currentBlock->isTerminated())
    {
        if (!node->elseBody)
        {
            continueAt = falseBranch;
        }
        else
        {
            continueAt = createBlock();
        }

        emit(new JumpInst(continueAt));
    }

    setBlock(falseBranch);
    if (node->elseBody)
    {
        node->elseBody->accept(this);

        if (!_currentBlock->isTerminated())
        {
            if (!continueAt)
                continueAt = createBlock();

            emit(new JumpInst(continueAt));
        }
    }

    if (continueAt)
    {
        setBlock(continueAt);
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
    contents << "Assertion failed at "
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

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    BasicBlock* prevLoopEntry = _currentLoopEntry;
    _currentLoopExit = loopExit;
    _currentLoopEntry = loopBegin;

    BasicBlock* loopBody = createBlock();
    _conditionalCodeGen.visitCondition(*node->condition, loopBody, loopExit);

    setBlock(loopBody);
    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(loopBegin));

    _currentLoopEntry = prevLoopEntry;
    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(ForNode* node)
{
    Value* iterable = visitAndGet(node->iterableExpression);
    Value* iter = getTraitMethodValue(node->iterableExpression->type, node->iter, node);
    Type* iterableType = getConcreteType(node->iterableExpression->type);
    Type* iteratorType = _astContext->symbolTable()->resolveAssociatedType("IteratorType", iterableType, node->iterableSymbol);
    assert(iteratorType);
    Value* next = getTraitMethodValue(iteratorType, node->next, node);

    BasicBlock* loopBegin = createBlock();
    BasicBlock* loopExit = createBlock();
    BasicBlock* isSome = createBlock();

    // Call rhs.iter()
    Value* iterator = createTemp(getValueType(iteratorType));
    emit(new CallInst(iterator, iter, {iterable}));

    emit(new JumpInst(loopBegin));
    setBlock(loopBegin);

    // Call iter.next()
    Value* nextOption = createTemp(getValueType(node->optionType));
    emit(new CallInst(nextOption, next, {iterator}));

    // Check for Some tag, and otherwise exit the loop
    size_t SomeTag = node->optionType->getValueConstructor("Some").first;
    Value* tag = createTemp(ValueType::U64);
    Value* tagOffset = _context->createConstantInt(ValueType::I64, offsetof(SplObject, constructorTag));
    emit(new IndexedLoadInst(tag, nextOption, tagOffset));
    emit(new ConditionalJumpInst(tag, "==", constant(SomeTag), isSome, loopExit));

    // Extract x from Some(x)
    setBlock(isSome);
    Value* varTemp = createTemp(getValueType(node->symbol->type));
    Value* varOffset = constant(sizeof(SplObject));
    emit(new IndexedLoadInst(varTemp, nextOption, varOffset));
    store(node->symbol, varTemp);

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    BasicBlock* prevLoopEntry = _currentLoopEntry;
    _currentLoopExit = loopExit;
    _currentLoopEntry = loopBegin;

    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(loopBegin));

    _currentLoopEntry = prevLoopEntry;
    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(IndexNode* node)
{
    Value* object = visitAndGet(node->object);
    Value* index = visitAndGet(node->index);
    Value* method = getTraitMethodValue(node->object->type, node->atMethod, node);

    node->value = createTemp(getValueType(node->type));

    emit(new CallInst(node->value, method, {object, index}));
}

void TACCodeGen::visit(ForeverNode* node)
{
    BasicBlock* loopBody = createBlock();
    BasicBlock* loopExit = createBlock();

    emit(new JumpInst(loopBody));
    setBlock(loopBody);

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    BasicBlock* prevLoopEntry = _currentLoopEntry;
    _currentLoopExit = loopExit;
    _currentLoopEntry = loopBody;

    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(loopBody));

    _currentLoopEntry = prevLoopEntry;
    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(BreakNode* node)
{
    emit(new JumpInst(_currentLoopExit));
}

void TACCodeGen::visit(ContinueNode* node)
{
    emit(new JumpInst(_currentLoopEntry));
}

void TACCodeGen::visit(AssignNode* node)
{
    Value* value = visitAndGet(node->rhs);

    TACAssignmentCodeGen assignCG(this, value);
    node->lhs->accept(&assignCG);
}

void TACAssignmentCodeGen::visit(NullaryNode* node)
{
    Symbol* symbol = node->symbol;
    assert(symbol->kind == kVariable);

    _mainCG->store(symbol, _value);
}

void TACAssignmentCodeGen::visit(MemberAccessNode* node)
{
    node->object->accept(_mainCG);

    Value* structure = node->object->value;

    uint64_t offsetInt = sizeof(SplObject) + 8 * node->memberIndex;
    Value* offset = _mainCG->_context->createConstantInt(ValueType::U64, offsetInt);
    _mainCG->emit(new IndexedStoreInst(structure, offset, _value));
}

void TACAssignmentCodeGen::visit(IndexNode* node)
{
    Value* object = _mainCG->visitAndGet(node->object);
    Value* index = _mainCG->visitAndGet(node->index);
    Value* method = _mainCG->getTraitMethodValue(node->object->type, node->setMethod, node);

    _mainCG->emit(new CallInst(_mainCG->createTemp(), method, {object, index, _value}));
}

void TACCodeGen::visit(VariableDefNode* node)
{
    if (!node->symbol)
    {
        visitAndGet(node->rhs);
    }
    else
    {
        Value* value = visitAndGet(node->rhs);
        store(node->symbol, value);
    }
}

void TACCodeGen::letHelper(LetNode* node, Value* rhs)
{
    // Copy over each of the members of the constructor pattern
    for (size_t i = 0; i < node->symbols.size(); ++i)
    {
        const Symbol* member = node->symbols.at(i);
        if (member)
        {
            ValueType type = getValueType(member->type);

            Value* tmp = createTemp(type);
            Value* offset = _context->createConstantInt(ValueType::I64, sizeof(SplObject) + 8 * i);
            emit(new IndexedLoadInst(tmp, rhs, offset));
            store(member, tmp);
        }
    }
}

void TACConditionalCodeGen::visit(LetNode* node)
{
    assert(node->isExpression);

    Value* rhs = visitAndGet(node->body);

    Value* tag = _mainCodeGen->createTemp(ValueType::U64);
    Value* offset = _context->createConstantInt(ValueType::I64, offsetof(SplObject, constructorTag));
    emit(new IndexedLoadInst(tag, rhs, offset));

    ValueConstructor* constructor = node->valueConstructor;
    size_t expectedTag = constructor->constructorTag();

    BasicBlock* setupBranch = createBlock();
    emit(new ConditionalJumpInst(tag, "==", _context->createConstantInt(ValueType::U64, expectedTag), setupBranch, _falseBranch));

    setBlock(setupBranch);
    _mainCodeGen->letHelper(node, rhs);
    emit(new JumpInst(_trueBranch));
}

void TACCodeGen::visit(LetNode* node)
{
    assert(!node->isExpression);

    Value* rhs = visitAndGet(node->body);
    letHelper(node, rhs);
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

    if (node->symbol->kind == kFunction && dynamic_cast<FunctionSymbol*>(node->symbol)->isBuiltin)
    {
        if (node->target == "not")
        {
            node->value = createTemp();

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

            node->value->type = getValueType(node->type);
            return;
        }
        else if (node->target == "arrayLength")
        {
            assert(arguments.size() == 1);

            node->value = createTemp();

            Value* array = arguments[0];
            Value* offset = _context->createConstantInt(ValueType::I64, offsetof(Array, numElements));
            emit(new IndexedLoadInst(node->value, array, offset));

            node->value->type = ValueType::U64;
            return;
        }
        else if (node->target == "unsafeArrayAt")
        {
            assert(arguments.size() == 2);

            node->value = createTemp();

            Value* array = arguments[0];
            Value* index = arguments[1];

            // Extract the type of the array elements
            ConstructedType* arrayType = node->arguments[0]->type->get<ConstructedType>();
            assert(arrayType->name() == "Array");
            assert(arrayType->typeParameters().size() == 1);
            ValueType eltType = getValueType(arrayType->typeParameters()[0]);

            Value* indexAfterHead = createTemp(ValueType::U64);
            Value* bytesPerElt = constant(getSize(eltType) / 8);
            emit(new BinaryOperationInst(indexAfterHead, index, BinaryOperation::MUL, bytesPerElt));

            Value* indexInBytes = createTemp(ValueType::U64);
            Value* sizeOfHeader = constant(sizeof(Array));
            emit(new BinaryOperationInst(indexInBytes, indexAfterHead, BinaryOperation::ADD, sizeOfHeader));

            emit(new IndexedLoadInst(node->value, array, indexInBytes));
            node->value->type = getValueType(node->type);
            return;
        }
        else if (node->target == "unsafeArraySet")
        {
            assert(arguments.size() == 3);

            Value* array = arguments[0];
            Value* index = arguments[1];
            Value* value = arguments[2];

            // Extract the type of the array elements
            ConstructedType* arrayType = node->arguments[0]->type->get<ConstructedType>();
            assert(arrayType->name() == "Array");
            assert(arrayType->typeParameters().size() == 1);
            ValueType eltType = getValueType(arrayType->typeParameters()[0]);

            Value* indexAfterHead = createTemp(ValueType::U64);
            Value* bytesPerElt = constant(getSize(eltType) / 8);
            emit(new BinaryOperationInst(indexAfterHead, index, BinaryOperation::MUL, bytesPerElt));

            Value* indexInBytes = createTemp(ValueType::U64);
            Value* sizeOfHeader = constant(sizeof(Array));
            emit(new BinaryOperationInst(indexInBytes, indexAfterHead, BinaryOperation::ADD, sizeOfHeader));

            emit(new IndexedStoreInst(array, indexInBytes, value));
            return;
        }
    }

    node->value = createTemp();
    Value* result = node->value;

    if (node->symbol->kind == kFunction)
    {
        Value* fn = getFunctionValue(node->symbol, node, node->typeAssignment);
        CallInst* inst = new CallInst(result, fn, arguments);

        FunctionSymbol* functionSymbol = dynamic_cast<FunctionSymbol*>(node->symbol);
        inst->ccall = functionSymbol->isExternal;
        inst->regpass = inst->ccall;
        emit(inst);
    }
    else if (node->symbol->kind == kMethod)
    {
        // Methods can't be ccall or regpass
        Value* fn = getFunctionValue(node->symbol, node, node->typeAssignment);
        emit(new CallInst(result, fn, arguments));
    }
    else if (node->symbol->kind == kTraitMethod)
    {
        // Static trait method
        Value* method = getTraitMethodValue(node->typeName->type, node->symbol, node, node->typeAssignment);
        emit(new CallInst(result, method, arguments));
    }
    else /* node->symbol->kind == kVariable */
    {
        // The variable represents a closure
        Value* closure = load(node->symbol);

        Value* fn = createTemp(ValueType::NonHeapAddress);
        emit(new IndexedLoadInst(fn, closure, constant(sizeof(SplObject))));

        Value* env = createTemp(ValueType::Reference);
        emit(new IndexedLoadInst(env, closure, constant(sizeof(SplObject) + 8)));

        arguments.push_back(env);

        CallInst* inst = new CallInst(result, fn, arguments);
        emit(inst);
    }

    result->type = getValueType(node->type);
}

void TACCodeGen::visit(BinopNode* node)
{
    Value* lhs = visitAndGet(node->lhs);
    Value* rhs = visitAndGet(node->rhs);
    node->value = createTemp(getValueType(node->type));

    // Overloaded operators
    if (node->method)
    {
        Value* method = getTraitMethodValue(node->lhs->type, node->method, node);
        emit(new CallInst(node->value, method, {lhs, rhs}));
        return;
    }

    // Otherwise, built-in numerical operator

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

    case BinopNode::kRem:
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

    node->value = createTemp();

    if (node->symbol->kind == kMethod)
    {
        Value* method = getFunctionValue(node->symbol, node, node->typeAssignment);
        emit(new CallInst(node->value, method, arguments));
    }
    else if (node->symbol->kind == kTraitMethod)
    {
        Value* method = getTraitMethodValue(node->object->type, node->symbol, node, node->typeAssignment);
        emit(new CallInst(node->value, method, arguments));
    }

    node->value->type = getValueType(node->type);
}

void TACCodeGen::visit(ReturnNode* node)
{
    Value* result = node->expression ? visitAndGet(node->expression) : nullptr;
    emit(new ReturnInst(result));
}

void TACCodeGen::visit(MemberAccessNode* node)
{
    node->value = createTemp(getValueType(node->type));

    node->object->accept(this);

    Value* structure = node->object->value;
    Value* offset = _context->createConstantInt(ValueType::I64, sizeof(SplObject) + 8 * node->memberIndex);
    emit(new IndexedLoadInst(node->value, structure, offset));
}

void TACCodeGen::visit(EnumDeclaration* node)
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
    std::unordered_map<MatchArm*, BasicBlock*> caseLabels;
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        caseLabels.emplace(node->arms[i], createBlock());
    }
    BasicBlock* continueAt = createBlock();

    Value* expr = visitAndGet(node->expr);

    // TODO: Handle case where all constructors are parameter-less

    Value* tag = createTemp(ValueType::U64);
    Value* offset = _context->createConstantInt(ValueType::I64, offsetof(SplObject, constructorTag));
    emit(new IndexedLoadInst(tag, expr, offset));

    // Jump to the appropriate case based on the tag
    BasicBlock* nextTest = _currentBlock;
    for (auto& item : caseLabels)
    {
        MatchArm* arm = item.first;
        BasicBlock* block = item.second;
        size_t armTag = arm->constructorTag;

        if (arm == node->catchallArm)
            continue;

        setBlock(nextTest);
        nextTest = createBlock();
        emit(new ConditionalJumpInst(tag, "==", constant(armTag), block, nextTest));
    }

    setBlock(nextTest);
    if (node->catchallArm)
    {
        emit(new JumpInst(caseLabels[node->catchallArm]));
    }
    else
    {
        // Match must be exhaustive, so we should never fail all tests
        emit(new UnreachableInst);
    }

    // Individual arms
    Value* lastSwitchExpr = _currentSwitchExpr;
    _currentSwitchExpr = expr;
    bool canReach = false;
    for (auto& item : caseLabels)
    {
        MatchArm* arm = item.first;
        BasicBlock* block = item.second;

        setBlock(block);
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
    if (node->constructorSymbol)
    {
        ValueConstructor* constructor = node->valueConstructor;

        // Copy over each of the members of the constructor pattern
        for (size_t i = 0; i < node->symbols.size(); ++i)
        {
            const Symbol* member = node->symbols.at(i);
            if (member)
            {
                Value* tmp = createTemp(getValueType(member->type));
                Value* offset = _context->createConstantInt(ValueType::I64, sizeof(SplObject) + 8 * i);
                emit(new IndexedLoadInst(tmp, _currentSwitchExpr, offset));
                store(member, tmp);
            }
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
    gcAllocate(result, size);

    //// Fill in the members with the constructor arguments

    // SplObject header fields
    uint64_t refMask = getConstructorLayout(symbol, nullptr, typeAssignment);

    emit(new IndexedStoreInst(result, constant(offsetof(SplObject, constructorTag)), constant(constructorTag)));
    emit(new IndexedStoreInst(result, constant(offsetof(SplObject, refMask)), constant(refMask)));

    // Individual members
    for (size_t i = 0; i < members.size(); ++i)
    {
        auto& member = members[i];

        std::string name = member.name;
        if (name.empty()) name = std::to_string(i);

        Value* param = _context->createArgument(getValueType(member.type, typeAssignment), name);
        _currentFunction->params.push_back(param);

        Value* temp = createTemp(getValueType(substitute(member.type, typeAssignment)));
        emit(new LoadInst(temp, param));
        emit(new IndexedStoreInst(result, constant(sizeof(SplObject) + 8 * i), temp));
    }

    emit(new ReturnInst(result));
}

void TACCodeGen::visit(ImplNode* node)
{
    AstVisitor::visit(node);
}

void TACCodeGen::makeArray(Type* functionType, bool zero)
{
    // Only argument = size in elements
    Value* size = _context->createArgument(ValueType::U64, "size");
    _currentFunction->params.push_back(size);

    // Extract the type of the array elements
    assert(functionType->tag() == ttFunction);
    Type* resultType = functionType->get<FunctionType>()->output();
    assert(resultType->tag() == ttConstructed);
    ConstructedType* arrayType = resultType->get<ConstructedType>();
    assert(arrayType->name() == "Array");
    assert(arrayType->typeParameters().size() == 1);
    ValueType eltType = getValueType(arrayType->typeParameters()[0]);

    Value* sizeAfterHead = createTemp(ValueType::U64);
    Value* bytesPerElt = constant(getSize(eltType) / 8);
    Value* tempSize = createTemp(ValueType::U64);
    emit(new LoadInst(tempSize, size));
    emit(new BinaryOperationInst(sizeAfterHead, tempSize, BinaryOperation::MUL, bytesPerElt));
    Value* sizeInBytes = createTemp(ValueType::U64);
    Value* sizeOfHeader = constant(sizeof(SplObject));
    emit(new BinaryOperationInst(sizeInBytes, sizeAfterHead, BinaryOperation::ADD, sizeOfHeader));

    // Allocate room for the object
    Value* result = createTemp(ValueType::Reference);
    gcAllocate(result, sizeInBytes);

    // Fill in the header information
    uint64_t constructorTag = eltType == ValueType::Reference ? BOXED_ARRAY_TAG : UNBOXED_ARRAY_TAG;

    emit(new IndexedStoreInst(
        result,
        constant(offsetof(Array, constructorTag)),
        constant(constructorTag)));

    emit(new IndexedStoreInst(result,
        constant(offsetof(Array, numElements)),
        tempSize));

    // Zero out all elements
    if (zero)
    {
        emit(new MemsetFn(
            result,
            sizeOfHeader,
            tempSize,
            _context->createConstantInt(eltType, 0)));
    }

    emit(new ReturnInst(result));
}
