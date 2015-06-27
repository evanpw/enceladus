#include "platform.hpp"
#include "tac_codegen.hpp"
#include "types.hpp"
#include "lib/library.h"

#include <iostream>

TACCodeGen::TACCodeGen()
: _conditionalCodeGen(this)
{
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
                result = new GlobalValue(symbol->name, GlobalTag::Static);
            }
            else if (symbol->asVariable()->isParam)
            {
                result = new Argument(symbol->name);
            }
            else if (symbol->enclosingFunction == nullptr)
            {
                result = new GlobalValue(symbol->name, GlobalTag::Variable);
            }
            else
            {
                // Local variable
                result = new Value(symbol->name);
            }
        }
        else if (symbol->kind == kFunction)
        {
            result = new Function(symbol->name);
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

    std::cerr << "(" << _currentBlock->str() << ")\t" << inst->str() << std::endl;
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
    Function* main = new Function("main");
    _currentFunction = _tacProgram.mainFunction = main;
    _nextSeqNumber = 0;
    _currentFunction->firstBlock = makeBlock();
    setBlock(_currentFunction->firstBlock);

    for (auto& child : node->children)
    {
        child->accept(this);
    }

    emit(new TACReturn());

    // The previous loop will have filled in _functions with a list of all
    // other functions. Now generate code for those
    for (FunctionDefNode* funcDefNode : _functions)
    {
        Function* function = new Function(funcDefNode->symbol->name);
        _tacProgram.otherFunctions.push_back(function);
        _currentFunction = function;
        _nextSeqNumber = 0;
        _currentFunction->firstBlock = makeBlock();
        setBlock(_currentFunction->firstBlock);

        // Collect all local variables
        for (auto& local : funcDefNode->scope->symbols.symbols)
        {
            const Symbol* symbol = local.second.get();

            if (symbol->kind == kVariable && !symbol->asVariable()->isParam)
            {
                _currentFunction->locals.push_back(getValue(symbol));
            }
        }

        // Collect all function parameters
        for (Symbol* param : funcDefNode->parameterSymbols)
        {
            assert(param->asVariable()->isParam);
            _currentFunction->params.push_back(getValue(param));
        }

        // Generate code for the function body
        std::cerr << "function " << funcDefNode->symbol->name << std::endl;
        funcDefNode->body->accept(this);

        // Handle implicit return values
        emit(new TACReturn(funcDefNode->body->value));

        std::cerr << "end function" << std::endl << std::endl;
    }

    for (DataDeclaration* dataDeclaration : _dataDeclarations)
    {
        for (size_t i = 0; i < dataDeclaration->valueConstructors.size(); ++i)
        {
            ValueConstructor* constructor = dataDeclaration->valueConstructors[i];

            // TODO: Fix this fake FunctionSymbol
            Function* function = new Function(constructor->name());
            _tacProgram.otherFunctions.push_back(function);
            _currentFunction = function;
            _nextSeqNumber = 0;
            _currentFunction->firstBlock = makeBlock();
            setBlock(_currentFunction->firstBlock);

            std::cerr << "constructor " << constructor->name() << std::endl;
            createConstructor(constructor, i);
            std::cerr << "end constructor" << std::endl << std::endl;
        }
    }

    for (StructDefNode* structDeclaration : _structDeclarations)
    {
        ValueConstructor* constructor = structDeclaration->valueConstructor;

        // TODO: Fix this fake FunctionSymbol
        Function* function = new Function(constructor->name());
        _tacProgram.otherFunctions.push_back(function);
        _currentFunction = function;
        _nextSeqNumber = 0;
        _currentFunction->firstBlock = makeBlock();
        setBlock(_currentFunction->firstBlock);

        std::cerr << "constructor " << constructor->name() << std::endl;
        createConstructor(structDeclaration->valueConstructor, 0);
        std::cerr << "end constructor" << std::endl << std::endl;
    }

    // Gather up all references to external functions and all global variables
    for (auto& i : node->scope->symbols.symbols)
    {
        const std::string& name = i.first;
        const Symbol* symbol = i.second.get();

        if (symbol->kind == kFunction && symbol->asFunction()->isExternal)
        {
            if (!symbol->asFunction()->isForeign)
            {
                _tacProgram.externs.push_back(name);
            }
            else
            {
                _tacProgram.externs.push_back(FOREIGN_NAME(name));
            }
        }
        else if (symbol->kind == kVariable)
        {
            _tacProgram.globals.push_back(getValue(symbol));
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
            emit(new TACConditionalJump(lhs, ">", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kLess:
            emit(new TACConditionalJump(lhs, "<", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kEqual:
            emit(new TACConditionalJump(lhs, "==", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kGreaterOrEqual:
            emit(new TACConditionalJump(lhs, ">=", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kLessOrEqual:
            emit(new TACConditionalJump(lhs, "<=", rhs, _trueBranch, _falseBranch));
            break;

        case ComparisonNode::kNotEqual:
            emit(new TACConditionalJump(lhs, "!=", rhs, _trueBranch, _falseBranch));
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

    if (!node->value)
        node->value = makeTemp();

    Value* lhs = visitAndGet(node->lhs);
    Value* rhs = visitAndGet(node->rhs);

    BasicBlock* trueBranch = makeBlock();
    BasicBlock* falseBranch = makeBlock();
    BasicBlock* continueAt = makeBlock();

    emit(new TACConditionalJump(lhs, operation, rhs, trueBranch, falseBranch));

    setBlock(falseBranch);
    emit(new TACAssign(node->value, ConstantInt::False));
    emit(new TACJump(continueAt));

    setBlock(trueBranch);
    emit(new TACAssign(node->value, ConstantInt::True));
    emit(new TACJump(continueAt));

    setBlock(continueAt);
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
    if (!node->value)
        node->value = makeTemp();

    Value* result = node->value;

    BasicBlock* continueAt = makeBlock();
    BasicBlock* testSecond = makeBlock();

    if (node->op == LogicalNode::kAnd)
    {
        BasicBlock* isTrue = makeBlock();

        emit(new TACAssign(result, ConstantInt::False));
        Value* lhs = visitAndGet(node->lhs);
        emit(new TACJumpIf(lhs, testSecond, continueAt));

        setBlock(testSecond);
        Value* rhs = visitAndGet(node->rhs);
        emit(new TACJumpIf(rhs, isTrue, continueAt));

        setBlock(isTrue);
        emit(new TACAssign(result, ConstantInt::True));
        emit(new TACJump(continueAt));
    }
    else if (node->op == LogicalNode::kOr)
    {
        BasicBlock* isFalse = makeBlock();

        emit(new TACAssign(result, ConstantInt::True));
        Value* lhs = visitAndGet(node->lhs);
        emit(new TACJumpIf(lhs, continueAt, testSecond));

        setBlock(testSecond);
        Value* rhs = visitAndGet(node->rhs);
        emit(new TACJumpIf(rhs, continueAt, isFalse));

        setBlock(isFalse);
        emit(new TACAssign(result, ConstantInt::False));
        emit(new TACJump(continueAt));
    }
    else
    {
        assert (false);
    }

    setBlock(continueAt);
}

void TACCodeGen::visit(NullaryNode* node)
{
    if (node->kind == NullaryNode::VARIABLE)
    {
        node->value = getValue(node->symbol);
    }
    else
    {
        if (!node->value)
            node->value = makeTemp();

        Value* dest = node->value;

        if (node->kind == NullaryNode::FOREIGN_CALL)
        {
            bool external = node->symbol->asFunction()->isExternal;
            emit(new TACCall(true, dest, getValue(node->symbol), {}, external));
        }
        else if (node->kind == NullaryNode::FUNC_CALL)
        {
            emit(new TACCall(false, dest, getValue(node->symbol)));
        }
        else /* node->kind == NullaryNode::CLOSURE */
        {
            // If the function is not completely applied, then this nullary node
            // evaluates to a function type -- create a closure
            size_t size = sizeof(SplObject) + 8;
            // TODO: Fix this fake FunctionSymbol
            emit(new TACCall(true, dest, new Function("gcAllocate"), {new ConstantInt(size)}));

            // SplObject header fields
            emit(new TACLeftIndexedAssignment(dest, offsetof(SplObject, constructorTag), ConstantInt::Zero));
            emit(new TACLeftIndexedAssignment(dest, offsetof(SplObject, sizeInWords), ConstantInt::Zero));

            // Address of the function as an unboxed member
            emit(new TACLeftIndexedAssignment(dest, sizeof(SplObject), getValue(node->symbol)));
        }
    }
}

void TACCodeGen::visit(IntNode* node)
{
    node->value = new ConstantInt(TO_INT(node->intValue));
}

void TACCodeGen::visit(BoolNode* node)
{
    if (node->boolValue)
    {
        node->value = ConstantInt::True;
    }
    else
    {
        node->value = ConstantInt::False;
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
    emit(new TACJump(continueAt));

    setBlock(continueAt);
}

void TACCodeGen::visit(IfElseNode* node)
{
    BasicBlock* trueBranch = makeBlock();
    BasicBlock* falseBranch = makeBlock();
    BasicBlock* continueAt = makeBlock();

    if (!node->value) node->value = makeTemp();

    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, falseBranch);

    setBlock(trueBranch);
    Value* bodyValue = visitAndGet(node->body);
    if (bodyValue) emit(new TACAssign(node->value, bodyValue));
    emit(new TACJump(continueAt));

    setBlock(falseBranch);
    Value* elseValue = visitAndGet(node->else_body);
    if (elseValue) emit(new TACAssign(node->value, elseValue));
    emit(new TACJump(continueAt));

    setBlock(continueAt);
}

void TACCodeGen::visit(WhileNode* node)
{
    BasicBlock* loopBegin = makeBlock();
    BasicBlock* loopExit = makeBlock();

    emit(new TACJump(loopBegin));
    setBlock(loopBegin);

    BasicBlock* loopBody = makeBlock();
    _conditionalCodeGen.visitCondition(*node->condition, loopBody, loopExit);

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    _currentLoopExit = loopExit;

    setBlock(loopBody);
    node->body->accept(this);
    emit(new TACJump(loopBegin));

    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(ForeverNode* node)
{
    BasicBlock* loopBody = makeBlock();
    BasicBlock* loopExit = makeBlock();

    emit(new TACJump(loopBody));
    setBlock(loopBody);

    // Push a new inner loop on the (implicit) stack
    BasicBlock* prevLoopExit = _currentLoopExit;
    _currentLoopExit = loopExit;

    node->body->accept(this);
    emit(new TACJump(loopBody));

    _currentLoopExit = prevLoopExit;

    setBlock(loopExit);
}

void TACCodeGen::visit(BreakNode* node)
{
    emit(new TACJump(_currentLoopExit));
    setBlock(makeBlock());
}

void TACCodeGen::visit(AssignNode* node)
{
    Value* dest = getValue(node->symbol);

    if (node->symbol->type->isBoxed())
    {
        Value* value = visitAndGet(node->rhs);
        emit(new TACAssign(dest, value));
    }
    else
    {
        // Try to make the rhs of this assignment construct its result directly
        // in the right spot
        node->rhs->value = getValue(node->symbol);
        Value* value = visitAndGet(node->rhs);

        // If it was stored somewhere else (for example, if no computation was
        // done, in a statement like x = y), then actually do the assignment
        if (value != getValue(node->symbol))
        {
            emit(new TACAssign(dest, value));
        }
    }
}

void TACCodeGen::visit(LetNode* node)
{
    Value* dest = getValue(node->symbol);

    if (!node->symbol)
    {
        visitAndGet(node->rhs);
    }
    else if (node->symbol->type->isBoxed())
    {
        Value* value = visitAndGet(node->rhs);
        emit(new TACAssign(dest, value));
    }
    else
    {
        // Try to make the rhs of this assignment construct its result directly
        // in the right spot
        node->rhs->value = dest;
        Value* value = visitAndGet(node->rhs);

        // If it was stored somewhere else (for example, if no computation was
        // done, in a statement like x = y), then actually do the assignment
        if (value != dest)
        {
            emit(new TACAssign(dest, value));
        }
    }
}

void TACCodeGen::visit(MatchNode* node)
{
    Value* body = visitAndGet(node->body);
    ValueConstructor* constructor = node->valueConstructor;

    // Copy over each of the members of the constructor pattern
    for (size_t i = 0; i < node->symbols.size(); ++i)
    {
        const Symbol* member = node->symbols.at(i);
        if (member)
        {
            size_t location = constructor->members().at(i).location;
            emit(new TACRightIndexedAssignment(getValue(member), body, sizeof(SplObject) + 8 * location));
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
        else if (node->target == "null")
        {
            assert(node->arguments.size() == 1);
            Value* arg = visitAndGet(node->arguments[0]);

            emit(new TACConditionalJump(arg, "==", ConstantInt::Zero, _trueBranch, _falseBranch));
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

    if (!node->value)
    {
        if (unwrap(node->type) != Type::Unit)
            node->value = makeTemp();
    }
    else
    {
        assert(unwrap(node->type) != Type::Unit);
    }

    Value* result = node->value;

    if (node->symbol->kind == kFunction && node->symbol->asFunction()->isBuiltin)
    {
        if (node->target == "not")
        {
            assert(arguments.size() == 1);

            BasicBlock* trueBranch = makeBlock();
            BasicBlock* falseBranch = makeBlock();
            BasicBlock* continueAt = makeBlock();

            emit(new TACJumpIf(arguments[0], trueBranch, falseBranch));

            setBlock(falseBranch);
            emit(new TACAssign(result, ConstantInt::True));
            emit(new TACJump(continueAt));

            setBlock(trueBranch);
            emit(new TACAssign(result, ConstantInt::False));

            setBlock(continueAt);
        }
        else if (node->target == "+")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::TADD, arguments[1]));
        }
        else if (node->target == "-")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::TSUB, arguments[1]));
        }
        else if (node->target == "*")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::TMUL, arguments[1]));
        }
        else if (node->target == "/")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::TDIV, arguments[1]));
        }
        else if (node->target == "%")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::TMOD, arguments[1]));
        }
        else
        {
            assert(false);
        }
    }
    else if (node->symbol->kind == kFunction)
    {
        bool external = node->symbol->asFunction()->isExternal;
        bool foreign = node->symbol->asFunction()->isForeign;
        emit(new TACCall(foreign, result, getValue(node->symbol), arguments, external));
    }
    else /* node->symbol->kind == kVariable */
    {
        // The variable represents a closure, so extract the actual function
        // address
        Value* functionAddress = makeTemp();
        emit(new TACRightIndexedAssignment(functionAddress, getValue(node->symbol), sizeof(SplObject)));
        emit(new TACIndirectCall(result, functionAddress, arguments));
    }
}

void TACCodeGen::visit(ReturnNode* node)
{
    Value* result = visitAndGet(node->expression);
    emit(new TACReturn(result));

    setBlock(makeBlock());
}

void TACCodeGen::visit(VariableNode* node)
{
    assert(node->symbol->kind == kVariable);
    node->value = getValue(node->symbol);
}

void TACCodeGen::visit(MemberAccessNode* node)
{
    Value* varAddress = getValue(node->varSymbol);

    if (!node->value)
        node->value = makeTemp();

    emit(new TACRightIndexedAssignment(node->value, varAddress, sizeof(SplObject) + 8 * node->memberLocation));
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

void TACCodeGen::visit(SwitchNode* node)
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
    Value* tag = makeTemp();

    BasicBlock* gotTag = makeBlock();
    BasicBlock* untagged = makeBlock();
    BasicBlock* tagged = makeBlock();

    // Check for immediate, and remove tag bit
    Value* checked = makeTemp();
    emit(new TACBinaryOperation(checked, expr, BinaryOperation::UAND, ConstantInt::One));
    emit(new TACConditionalJump(checked, "==", ConstantInt::Zero, untagged, tagged));

    setBlock(tagged);
    emit(new TACBinaryOperation(tag, expr, BinaryOperation::SHR, ConstantInt::One));
    emit(new TACJump(gotTag));

    // Otherwise, extract tag from object header
    setBlock(untagged);
    emit(new TACRightIndexedAssignment(tag, expr, offsetof(SplObject, constructorTag)));
    emit(new TACJump(gotTag));

    setBlock(gotTag);

    // Jump to the appropriate case based on the tag
    BasicBlock* nextTest = _currentBlock;
    for (int i = 0; i < node->arms.size(); ++i)
    {
        auto& arm = node->arms[i];
        size_t armTag = arm->constructorTag;

        setBlock(nextTest);
        nextTest = makeBlock();
        emit(new TACConditionalJump(tag, "==", new ConstantInt(armTag), caseLabels[i], nextTest));
    }

    // Individual arms
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        auto& arm = node->arms[i];

        // The arm needs to know the expression so that it can extract fields
        arm->value = expr;

        setBlock(caseLabels[i]);
        arm->accept(this);
        emit(new TACJump(continueAt));
    }

    setBlock(continueAt);
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
            emit(new TACRightIndexedAssignment(getValue(member), node->value, sizeof(SplObject) + 8 * location));
        }
    }

    node->body->accept(this);
}

void TACCodeGen::visit(StringLiteralNode* node)
{
    node->value = getValue(node->symbol);
    _tacProgram.staticStrings.emplace_back(node->value, node->content);
}

void TACCodeGen::createConstructor(ValueConstructor* constructor, size_t constructorTag)
{
    const std::vector<ValueConstructor::MemberDesc> members = constructor->members();

    // Value constructors with no parameters are represented as immediates
    if (members.size() == 0)
    {
        emit(new TACReturn(new ConstantInt(TO_INT(constructorTag))));
        return;
    }

    Value* result = makeTemp();

    // For now, every member takes up exactly 8 bytes (either directly or as a pointer).
    size_t size = sizeof(SplObject) + 8 * members.size();

    // Allocate room for the object
    emit(new TACCall(
        true,
        result,
        new Function("gcAllocate"), // TODO: Fix this
        {new ConstantInt(size)}));

    //// Fill in the members with the constructor arguments

    // SplObject header fields
    emit(new TACLeftIndexedAssignment(result, offsetof(SplObject, constructorTag), new ConstantInt(constructorTag)));
    emit(new TACLeftIndexedAssignment(result, offsetof(SplObject, sizeInWords), new ConstantInt(members.size())));

    // Individual members
    for (size_t i = 0; i < members.size(); ++i)
    {
        auto& member = members[i];
        size_t location = member.location;

        Value* param;
        if (member.name.empty())
        {
            param = makeTemp();
        }
        else
        {
            param = new Value(member.name);
        }

        _currentFunction->regParams.push_back(param);
        _currentFunction->locals.push_back(param);

        emit(new TACLeftIndexedAssignment(result, sizeof(SplObject) + 8 * location, param));
    }

    emit(new TACReturn(result));
}
