#include "ir/tac_codegen.hpp"

#include "ast/ast_context.hpp"
#include "lib/library.h"
#include "semantic/types.hpp"

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

static ValueType getValueType(Type* type)
{
    if (type->isBoxed())
    {
        return ValueType::BoxOrInt;
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

    auto i = _names.find(symbol);
    if (i != _names.end())
    {
        return i->second;
    }
    else
    {
        Value* result;
        if (symbol->kind == kVariable)
        {
            if (symbol->asVariable()->isStatic)
            {
                result = _context->makeStaticString(symbol->name, symbol->asVariable()->contents);
            }
            else if (symbol->asVariable()->isParam)
            {
                ValueType type = getValueType(symbol->type);
                result = _context->makeArgument(type, symbol->name);
            }
            else if (symbol->global)
            {
                ValueType type = getValueType(symbol->type);
                result = _context->makeGlobal(type, symbol->name);
            }
            else
            {
                ValueType type = getValueType(symbol->type);
                result = _context->makeLocal(type, symbol->name);
                _currentFunction->locals.push_back(result);
            }
        }
        else if (symbol->kind == kFunction)
        {
            const FunctionSymbol* functionSymbol = symbol->asFunction();
            if (functionSymbol->isExternal)
            {
                result = _context->makeExternFunction(symbol->name);
            }
            else
            {
                result = _context->makeFunction(symbol->name);
            }
        }
        else
        {
            assert(false);
        }

        _names[symbol] = result;
        return result;
    }
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

BasicBlock* TACConditionalCodeGen::makeBlock()
{
    return _mainCodeGen->makeBlock();
}

void TACCodeGen::visit(ProgramNode* node)
{
    Function* main = _context->makeFunction("splmain");
    _currentFunction = main;
    _nextSeqNumber = 0;
    setBlock(makeBlock());

    for (auto& child : node->children)
    {
        child->accept(this);
    }

    emit(new ReturnInst());

    // The previous loop will have filled in _functions with a list of all
    // other functions. Now generate code for those
    for (FunctionDefNode* funcDefNode : _functions)
    {
        Function* function = (Function*)getValue(funcDefNode->symbol);
        _currentFunction = function;
        _nextSeqNumber = 0;
        setBlock(makeBlock());

        // Collect all function parameters
        for (Symbol* param : funcDefNode->parameterSymbols)
        {
            assert(param->asVariable()->isParam);
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

    for (DataDeclaration* dataDeclaration : _dataDeclarations)
    {
        for (size_t i = 0; i < dataDeclaration->valueConstructors.size(); ++i)
        {
            ValueConstructor* constructor = dataDeclaration->valueConstructors[i];

            Function* function = (Function*)getValue(constructor->symbol());
            _currentFunction = function;
            _nextSeqNumber = 0;
            setBlock(makeBlock());

            createConstructor(constructor, i);
        }
    }

    for (StructDefNode* structDeclaration : _structDeclarations)
    {
        ValueConstructor* constructor = structDeclaration->valueConstructor;

        Function* function = (Function*)getValue(constructor->symbol());
        _currentFunction = function;
        _nextSeqNumber = 0;
        setBlock(makeBlock());

        createConstructor(structDeclaration->valueConstructor, 0);
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

    BasicBlock* trueBranch = makeBlock();
    BasicBlock* falseBranch = makeBlock();
    BasicBlock* continueAt = makeBlock();

    emit(new ConditionalJumpInst(lhs, operation, rhs, trueBranch, falseBranch));

    setBlock(falseBranch);
    emit(new JumpInst(continueAt));

    setBlock(trueBranch);
    emit(new JumpInst(continueAt));

    setBlock(continueAt);
    node->value = makeTemp(ValueType::Integer);
    PhiInst* phi = new PhiInst(node->value);
    phi->addSource(falseBranch, _context->False);
    phi->addSource(trueBranch, _context->True);
    emit(phi);
}

void TACConditionalCodeGen::visit(LogicalNode* node)
{
    if (node->op == LogicalNode::kAnd)
    {
        BasicBlock* firstTrue = makeBlock();
        visitCondition(*node->lhs, firstTrue, _falseBranch);

        setBlock(firstTrue);
        visitCondition(*node->rhs, _trueBranch, _falseBranch);
    }
    else if (node->op == LogicalNode::kOr)
    {
        BasicBlock* firstFalse = makeBlock();
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
    BasicBlock* continueAt = makeBlock();
    BasicBlock* testSecond = makeBlock();
    BasicBlock* trueBranch = makeBlock();
    BasicBlock* falseBranch = makeBlock();

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
    node->value = makeTemp(ValueType::Integer);
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
        node->value = makeTemp(rhs->type);
        emit(new LoadInst(node->value, rhs));
    }
    else
    {
        ValueType type = getValueType(node->type);
        Value* dest = node->value = makeTemp(type);

        if (node->kind == NullaryNode::FOREIGN_CALL)
        {
            CallInst* inst = new CallInst(dest, getValue(node->symbol), {});
            inst->foreign = true;
            inst->ccall = node->symbol->asFunction()->isExternal;
            inst->regpass = inst->ccall;
            emit(inst);
        }
        else if (node->kind == NullaryNode::FUNC_CALL)
        {
            emit(new CallInst(dest, getValue(node->symbol)));
        }
        else /* node->kind == NullaryNode::CLOSURE */
        {
            // If the function is not completely applied, then this nullary node
            // evaluates to a function type -- create a closure
            size_t size = sizeof(SplObject) + 8;
            CallInst* callInst = new CallInst(dest, _context->makeExternFunction("gcAllocate"), {_context->getConstantInt(size)});
            callInst->foreign = true;
            callInst->regpass = true;
            emit(callInst);

            // SplObject header fields
            emit(new IndexedStoreInst(dest, offsetof(SplObject, constructorTag), _context->Zero));
            emit(new IndexedStoreInst(dest, offsetof(SplObject, sizeInWords), _context->Zero));

            // Address of the function as an unboxed member
            emit(new IndexedStoreInst(dest, sizeof(SplObject), getValue(node->symbol)));
        }
    }
}

void TACCodeGen::visit(IntNode* node)
{
    node->value = _context->getConstantInt(TO_INT(node->intValue));
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
    BasicBlock* trueBranch = makeBlock();
    BasicBlock* continueAt = makeBlock();

    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, continueAt);

    setBlock(trueBranch);
    node->body->accept(this);

    if (!_currentBlock->isTerminated())
        emit(new JumpInst(continueAt));

    setBlock(continueAt);
}

void TACCodeGen::visit(IfElseNode* node)
{
    BasicBlock* trueBranch = makeBlock();
    BasicBlock* falseBranch = makeBlock();

    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, falseBranch);

    BasicBlock* continueAt = nullptr;

    setBlock(trueBranch);
    Value* bodyValue = visitAndGet(node->body);
    if (!_currentBlock->isTerminated())
    {
        continueAt = makeBlock();

        trueBranch = _currentBlock;
        emit(new JumpInst(continueAt));
    }

    setBlock(falseBranch);
    Value* elseValue = visitAndGet(node->elseBody);
    if (!_currentBlock->isTerminated())
    {
        if (!continueAt)
            continueAt = makeBlock();

        falseBranch = _currentBlock;
        emit(new JumpInst(continueAt));
    }

    if (continueAt)
    {
        setBlock(continueAt);
        if (bodyValue)
        {
            assert(elseValue);

            node->value = makeTemp(bodyValue->type);
            PhiInst* phi = new PhiInst(node->value);
            phi->addSource(trueBranch, bodyValue);
            phi->addSource(falseBranch, elseValue);
            emit(phi);
        }
    }
}

void TACCodeGen::visit(WhileNode* node)
{
    BasicBlock* loopBegin = makeBlock();
    BasicBlock* loopExit = makeBlock();

    emit(new JumpInst(loopBegin));
    setBlock(loopBegin);

    BasicBlock* loopBody = makeBlock();
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
    Value* headFunction = getValue(node->headSymbol);
    Value* tailFunction = getValue(node->tailSymbol);
    Value* nullFunction = getValue(node->nullSymbol);


    BasicBlock* loopInit = makeBlock();
    BasicBlock* loopBegin = makeBlock();
    BasicBlock* loopExit = makeBlock();
    BasicBlock* loopBody = makeBlock();

    // Create an unnamed local variable to hold the list being iterated over
    Value* listVar = _context->makeLocal(ValueType::BoxOrInt, "");
    _currentFunction->locals.push_back(listVar);

    emit(new JumpInst(loopInit));
    setBlock(loopInit);

    Value* initialList = visitAndGet(node->listExpression);
    emit(new StoreInst(listVar, initialList));

    emit(new JumpInst(loopBegin));
    setBlock(loopBegin);

    // Loop while list variable is not null
    Value* currentList = makeTemp(ValueType::BoxOrInt);
    emit(new LoadInst(currentList, listVar));
    Value* isNull = makeTemp(ValueType::Integer);
    emit(new CallInst(isNull, nullFunction, {currentList}));
    emit(new JumpIfInst(isNull, loopExit, loopBody));

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    _currentLoopExit = loopExit;

    setBlock(loopBody);

    // Assign the head of the list to the induction variable
    currentList = makeTemp(ValueType::BoxOrInt);
    emit(new LoadInst(currentList, listVar));
    Value* currentHead = makeTemp(ValueType::BoxOrInt); // TODO: Check for list of value types
    emit(new CallInst(currentHead, headFunction, {currentList}));
    emit(new StoreInst(getValue(node->symbol), currentHead));

    // Pop the head off the current list
    Value* currentTail = makeTemp(ValueType::BoxOrInt);
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
    BasicBlock* loopInit = makeBlock();
    BasicBlock* loopBegin = makeBlock();
    BasicBlock* loopExit = makeBlock();
    BasicBlock* loopBody = makeBlock();

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
    Value* current = makeTemp(ValueType::Integer);
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
        Value* current = makeTemp(ValueType::Integer);
        emit(new LoadInst(current, inductionVar));
        // Add 2 directly to the tagged integer: equivalent to untagging, adding 1, and re-tagging
        Value* next = makeTemp(ValueType::Integer);
        emit(new BinaryOperationInst(next, current, BinaryOperation::ADD, _context->getConstantInt(2)));
        emit(new StoreInst(inductionVar, next));

        emit(new JumpInst(loopBegin));
    }

    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(ForeverNode* node)
{
    BasicBlock* loopBody = makeBlock();
    BasicBlock* loopExit = makeBlock();

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
    Value* dest = getValue(node->symbol);
    Value* value = visitAndGet(node->rhs);

    emit(new StoreInst(dest, value));
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

    // Copy over each of the members of the constructor pattern
    for (size_t i = 0; i < node->symbols.size(); ++i)
    {
        const Symbol* member = node->symbols.at(i);
        if (member)
        {
            ValueType type = getValueType(member->type);

            size_t location = constructor->members().at(i).location;
            Value* tmp = makeTemp(type);
            emit(new IndexedLoadInst(tmp, body, sizeof(SplObject) + 8 * location));
            emit(new StoreInst(getValue(member), tmp));
        }
    }
}

void TACConditionalCodeGen::visit(FunctionCallNode* node)
{
    if (node->symbol->kind == kFunction && node->symbol->asFunction()->isBuiltin)
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
        node->value = makeTemp(getValueType(node->type));

    Value* result = node->value;

    if (node->symbol->kind == kFunction && node->symbol->asFunction()->isBuiltin)
    {
        if (node->target == "not")
        {
            assert(arguments.size() == 1);

            BasicBlock* trueBranch = makeBlock();
            BasicBlock* falseBranch = makeBlock();
            BasicBlock* continueAt = makeBlock();

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

        }
        else if (node->target == "+")
        {
            assert(arguments.size() == 2);
            Value* lhs = makeTemp(ValueType::Integer);
            Value* rhs = makeTemp(ValueType::Integer);
            Value* tempResult = makeTemp(ValueType::Integer);
            emit(new UntagInst(lhs, arguments[0]));
            emit(new UntagInst(rhs, arguments[1]));
            emit(new BinaryOperationInst(tempResult, lhs, BinaryOperation::ADD, rhs));
            emit(new TagInst(result, tempResult));

        }
        else if (node->target == "-")
        {
            assert(arguments.size() == 2);
            Value* lhs = makeTemp(ValueType::Integer);
            Value* rhs = makeTemp(ValueType::Integer);
            Value* tempResult = makeTemp(ValueType::Integer);
            emit(new UntagInst(lhs, arguments[0]));
            emit(new UntagInst(rhs, arguments[1]));
            emit(new BinaryOperationInst(tempResult, lhs, BinaryOperation::SUB, rhs));
            emit(new TagInst(result, tempResult));
        }
        else if (node->target == "*")
        {
            assert(arguments.size() == 2);
            Value* lhs = makeTemp(ValueType::Integer);
            Value* rhs = makeTemp(ValueType::Integer);
            Value* tempResult = makeTemp(ValueType::Integer);
            emit(new UntagInst(lhs, arguments[0]));
            emit(new UntagInst(rhs, arguments[1]));
            emit(new BinaryOperationInst(tempResult, lhs, BinaryOperation::MUL, rhs));
            emit(new TagInst(result, tempResult));
        }
        else if (node->target == "/")
        {
            assert(arguments.size() == 2);
            Value* lhs = makeTemp(ValueType::Integer);
            Value* rhs = makeTemp(ValueType::Integer);
            Value* tempResult = makeTemp(ValueType::Integer);
            emit(new UntagInst(lhs, arguments[0]));
            emit(new UntagInst(rhs, arguments[1]));
            emit(new BinaryOperationInst(tempResult, lhs, BinaryOperation::DIV, rhs));
            emit(new TagInst(result, tempResult));
        }
        else if (node->target == "%")
        {
            assert(arguments.size() == 2);
            Value* lhs = makeTemp(ValueType::Integer);
            Value* rhs = makeTemp(ValueType::Integer);
            Value* tempResult = makeTemp(ValueType::Integer);
            emit(new UntagInst(lhs, arguments[0]));
            emit(new UntagInst(rhs, arguments[1]));
            emit(new BinaryOperationInst(tempResult, lhs, BinaryOperation::MOD, rhs));
            emit(new TagInst(result, tempResult));
        }
        else
        {
            assert(false);
        }
    }
    else if (node->symbol->kind == kFunction)
    {
        CallInst* inst = new CallInst(result, getValue(node->symbol), arguments);
        inst->foreign = node->symbol->asFunction()->isForeign;
        inst->ccall = node->symbol->asFunction()->isExternal;
        inst->regpass = inst->ccall;
        emit(inst);
    }
    else /* node->symbol->kind == kVariable */
    {
        // The variable represents a closure, so extract the actual function
        // address
        Value* functionAddress = makeTemp(ValueType::CodeAddress);
        Value* closure = makeTemp(ValueType::BoxOrInt);
        emit(new LoadInst(closure, getValue(node->symbol)));
        emit(new IndexedLoadInst(functionAddress, closure, sizeof(SplObject)));

        CallInst* inst = new CallInst(result, functionAddress, arguments);
        emit(inst);
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
    node->value = makeTemp(getValueType(node->symbol->type));
    emit(new LoadInst(node->value, getValue(node->symbol)));
}

void TACCodeGen::visit(MemberAccessNode* node)
{
    node->value = makeTemp(getValueType(node->memberSymbol->type));

    Value* structure = makeTemp(getValueType(node->varSymbol->type));
    emit(new LoadInst(structure, getValue(node->varSymbol)));
    emit(new IndexedLoadInst(node->value, structure, sizeof(SplObject) + 8 * node->memberLocation));
}

void TACCodeGen::visit(StructDefNode* node)
{
    _structDeclarations.push_back(node);
}

void TACCodeGen::visit(MemberDefNode* node)
{
}

void TACCodeGen::visit(TypeAliasNode* node)
{
}

void TACCodeGen::visit(FunctionDefNode* node)
{
    // Do the code generation for this function later, after we've generated
    // code for the main function
    _functions.push_back(node);
}

void TACCodeGen::visit(DataDeclaration* node)
{
    _dataDeclarations.push_back(node);
}

void TACCodeGen::visit(MatchNode* node)
{
    std::vector<BasicBlock*> caseLabels;
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        caseLabels.push_back(makeBlock());
    }
    BasicBlock* continueAt = makeBlock();

    Value* expr = visitAndGet(node->expr);

    // Get constructor tag of expr (either as a immediate, or from the object
    // header)
    BasicBlock* gotTag = makeBlock();
    BasicBlock* isImmediate = makeBlock();
    BasicBlock* isObject = makeBlock();

    // TODO: Handle case where all constructors are parameter-less

    // Check for immediate, and remove tag bit
    Value* checked = makeTemp(ValueType::Integer);
    emit(new BinaryOperationInst(checked, expr, BinaryOperation::AND, _context->One));
    emit(new ConditionalJumpInst(checked, "==", _context->Zero, isObject, isImmediate));

    setBlock(isImmediate);
    Value* tag1 = makeTemp(ValueType::Integer);
    emit(new BinaryOperationInst(tag1, expr, BinaryOperation::SHR, _context->One));
    emit(new JumpInst(gotTag));

    // Otherwise, extract tag from object header
    setBlock(isObject);
    Value* tag2 = makeTemp(ValueType::Integer);
    emit(new IndexedLoadInst(tag2, expr, offsetof(SplObject, constructorTag)));
    emit(new JumpInst(gotTag));

    setBlock(gotTag);
    Value* tag = makeTemp(ValueType::Integer);
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
        nextTest = makeBlock();
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

    // Copy over each of the members of the constructor pattern
    for (size_t i = 0; i < node->symbols.size(); ++i)
    {
        const Symbol* member = node->symbols.at(i);
        if (member)
        {
            size_t location = constructor->members().at(i).location;

            Value* tmp = makeTemp(getValueType(member->type));
            emit(new IndexedLoadInst(tmp, _currentSwitchExpr, sizeof(SplObject) + 8 * location));
            emit(new StoreInst(getValue(member), tmp));
        }
    }

    node->body->accept(this);
}

void TACCodeGen::visit(StringLiteralNode* node)
{
    node->value = getValue(node->symbol);
}

void TACCodeGen::createConstructor(ValueConstructor* constructor, size_t constructorTag)
{
    const std::vector<ValueConstructor::MemberDesc> members = constructor->members();

    // Value constructors with no parameters are represented as immediates
    if (members.size() == 0)
    {
        emit(new ReturnInst(_context->getConstantInt(TO_INT(constructorTag))));
        return;
    }

    Value* result = makeTemp(ValueType::BoxOrInt);

    // For now, every member takes up exactly 8 bytes (either directly or as a pointer).
    size_t size = sizeof(SplObject) + 8 * members.size();

    // Allocate room for the object
    CallInst* inst = new CallInst(
        result,
        _context->makeExternFunction("gcAllocate"), // TODO: Fix this
        {_context->getConstantInt(size)});
    inst->foreign = true;
    inst->regpass = true;
    emit(inst);

    //// Fill in the members with the constructor arguments

    // SplObject header fields
    emit(new IndexedStoreInst(result, offsetof(SplObject, constructorTag), _context->getConstantInt(constructorTag)));
    emit(new IndexedStoreInst(result, offsetof(SplObject, sizeInWords), _context->getConstantInt(members.size())));

    // Individual members
    for (size_t i = 0; i < members.size(); ++i)
    {
        auto& member = members[i];
        size_t location = member.location;

        std::string name = member.name;
        if (name.empty()) name = std::to_string(i);

        Value* param = _context->makeArgument(getValueType(member.type), name);
        _currentFunction->params.push_back(param);

        Value* temp = makeTemp(getValueType(member.type));
        emit(new LoadInst(temp, param));
        emit(new IndexedStoreInst(result, sizeof(SplObject) + 8 * location, temp));
    }

    emit(new ReturnInst(result));
}
