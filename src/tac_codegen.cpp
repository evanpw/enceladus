#include "platform.hpp"
#include "tac_codegen.hpp"
#include "types.hpp"
#include "lib/library.h"

#include <iostream>

TACCodeGen::TACCodeGen()
: _conditionalCodeGen(this)
{
}

std::shared_ptr<Address> TACCodeGen::getNameAddress(const Symbol* symbol)
{
    auto i = _names.find(symbol);
    if (i != _names.end())
    {
        return i->second;
    }
    else
    {
        _names[symbol] = std::make_shared<NameAddress>(symbol);
        return _names[symbol];
    }
}

void TACConditionalCodeGen::emit(TACInstruction* inst)
{
    _mainCodeGen->emit(inst);
}

void TACCodeGen::emit(TACInstruction* inst)
{
    if (!_currentInstruction)
    {
        _currentFunction->instructions = inst;
    }
    else
    {
        _currentInstruction->next = inst;
    }

    _currentInstruction = inst;
}

std::shared_ptr<Address> TACConditionalCodeGen::visitAndGet(AstNode& node)
{
    return _mainCodeGen->visitAndGet(node);
}

void TACCodeGen::visit(ProgramNode* node)
{
    _currentFunction = &_tacProgram.mainFunction;
    _currentFunction->returnValue = Address::Null; // No return value
    _currentInstruction = nullptr;

    for (auto& child : node->children)
    {
        child->accept(this);
    }

    // The previous loop will have filled in _functions with a list of all
    // other functions. Now generate code for those
    for (FunctionDefNode* funcDefNode : _functions)
    {
        _tacProgram.otherFunctions.emplace_back(funcDefNode->symbol->name);

        _currentFunction = &_tacProgram.otherFunctions.back();
        _currentFunction->returnValue = makeTemp();
        _functionEnd = new TACLabel;
        _currentInstruction = nullptr;

        // Collect all local variables
        for (auto& local : funcDefNode->scope->symbols.symbols)
        {
            const Symbol* symbol = local.second.get();

            if (symbol->kind == kVariable && !symbol->asVariable()->isParam)
            {
                _currentFunction->locals.push_back(getNameAddress(symbol));
            }
        }

        // Collect all function parameters
        for (Symbol* param : funcDefNode->parameterSymbols)
        {
            assert(param->asVariable()->isParam);
            std::shared_ptr<Address> paramAddress = getNameAddress(param);
            _currentFunction->params.push_back(paramAddress);
        }

        // Generate code for the function body
        funcDefNode->body->accept(this);

        // Handle implicit return values
        if (funcDefNode->body->address)
        {
            emit(new TACAssign(_currentFunction->returnValue, funcDefNode->body->address));
        }

        emit(_functionEnd);
    }

    for (DataDeclaration* dataDeclaration : _dataDeclarations)
    {
        for (size_t i = 0; i < dataDeclaration->valueConstructors.size(); ++i)
        {
            ValueConstructor* constructor = dataDeclaration->valueConstructors[i];
            _tacProgram.otherFunctions.emplace_back(constructor->name());
            _currentFunction = &_tacProgram.otherFunctions.back();
            _currentInstruction = nullptr;

            createConstructor(constructor, i);
        }
    }

    for (StructDefNode* structDeclaration : _structDeclarations)
    {
        ValueConstructor* constructor = structDeclaration->valueConstructor;

        _tacProgram.otherFunctions.emplace_back(constructor->name());
        _currentFunction = &_tacProgram.otherFunctions.back();
        _currentInstruction = nullptr;

        createConstructor(structDeclaration->valueConstructor, 0);
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
            _tacProgram.globals.push_back(getNameAddress(symbol));
        }
    }
}

void TACConditionalCodeGen::visit(ComparisonNode* node)
{
    std::shared_ptr<Address> lhs = visitAndGet(*node->lhs);
    std::shared_ptr<Address> rhs = visitAndGet(*node->rhs);

    switch(node->op)
    {
        case ComparisonNode::kGreater:
            emit(new TACConditionalJump(lhs, ">", rhs, _trueBranch));
            break;

        case ComparisonNode::kLess:
            emit(new TACConditionalJump(lhs, "<", rhs, _trueBranch));
            break;

        case ComparisonNode::kEqual:
            emit(new TACConditionalJump(lhs, "==", rhs, _trueBranch));
            break;

        case ComparisonNode::kGreaterOrEqual:
            emit(new TACConditionalJump(lhs, ">=", rhs, _trueBranch));
            break;

        case ComparisonNode::kLessOrEqual:
            emit(new TACConditionalJump(lhs, "<=", rhs, _trueBranch));
            break;

        case ComparisonNode::kNotEqual:
            emit(new TACConditionalJump(lhs, "!=", rhs, _trueBranch));
            break;

        default: assert(false);
    }

    emit(new TACJump(_falseBranch));
}

void TACCodeGen::visit(ComparisonNode* node)
{
    std::shared_ptr<Address> lhs = visitAndGet(*node->lhs);
    std::shared_ptr<Address> rhs = visitAndGet(*node->rhs);

    TACLabel* trueBranch = new TACLabel;

    switch(node->op)
    {
        case ComparisonNode::kGreater:
            emit(new TACConditionalJump(lhs, ">", rhs, trueBranch));
            break;

        case ComparisonNode::kLess:
            emit(new TACConditionalJump(lhs, "<", rhs, trueBranch));
            break;

        case ComparisonNode::kEqual:
            emit(new TACConditionalJump(lhs, "==", rhs, trueBranch));
            break;

        case ComparisonNode::kGreaterOrEqual:
            emit(new TACConditionalJump(lhs, ">=", rhs, trueBranch));
            break;

        case ComparisonNode::kLessOrEqual:
            emit(new TACConditionalJump(lhs, "<=", rhs, trueBranch));
            break;

        case ComparisonNode::kNotEqual:
            emit(new TACConditionalJump(lhs, "!=", rhs, trueBranch));
            break;

        default: assert(false);
    }

    if (!node->address)
        node->address = makeTemp();

    TACLabel* endLabel = new TACLabel;

    emit(new TACAssign(node->address, ConstAddress::False));
    emit(new TACJump(endLabel));
    emit(trueBranch);
    emit(new TACAssign(node->address, ConstAddress::True));
    emit(endLabel);
}

void TACConditionalCodeGen::visit(LogicalNode* node)
{
    if (node->op == LogicalNode::kAnd)
    {
        TACLabel* firstTrue = new TACLabel;
        visitCondition(*node->lhs, firstTrue, _falseBranch);

        emit(firstTrue);
        visitCondition(*node->rhs, _trueBranch, _falseBranch);
    }
    else if (node->op == LogicalNode::kOr)
    {
        TACLabel* firstFalse = new TACLabel;
        visitCondition(*node->lhs, _trueBranch, firstFalse);

        emit(firstFalse);
        visitCondition(*node->rhs, _trueBranch, _falseBranch);
    }
    else
    {
        assert(false);
    }
}

void TACCodeGen::visit(LogicalNode* node)
{
    if (!node->address)
        node->address = makeTemp();

    std::shared_ptr<Address> result = node->address;

    TACLabel* endLabel = new TACLabel;

    if (node->op == LogicalNode::kAnd)
    {
        emit(new TACAssign(result, ConstAddress::False));
        std::shared_ptr<Address> lhs = visitAndGet(*node->lhs);
        emit(new TACJumpIfNot(lhs, endLabel));
        std::shared_ptr<Address> rhs = visitAndGet(*node->rhs);
        emit(new TACJumpIfNot(rhs, endLabel));
        emit(new TACAssign(result, ConstAddress::True));
        emit(endLabel);
    }
    else if (node->op == LogicalNode::kOr)
    {
        emit(new TACAssign(result, ConstAddress::True));
        std::shared_ptr<Address> lhs = visitAndGet(*node->lhs);
        emit(new TACJumpIf(lhs, endLabel));
        std::shared_ptr<Address> rhs = visitAndGet(*node->rhs);
        emit(new TACJumpIf(rhs, endLabel));
        emit(new TACAssign(result, ConstAddress::False));
        emit(endLabel);
    }
    else
    {
        assert (false);
    }
}

void TACCodeGen::visit(NullaryNode* node)
{
    if (node->kind == NullaryNode::VARIABLE)
    {
        node->address = getNameAddress(node->symbol);
    }
    else
    {
        if (!node->address)
            node->address = makeTemp();

        std::shared_ptr<Address> dest = node->address;

        if (node->kind == NullaryNode::FOREIGN_CALL)
        {
            std::string name;
            bool external = node->symbol->asFunction()->isExternal;
            if (external)
            {
                name = FOREIGN_NAME(node->symbol->name);
            }
            else
            {
                name = mangle(node->symbol->name);
            }

            emit(new TACCall(true, dest, name, {}, external));
        }
        else if (node->kind == NullaryNode::FUNC_CALL)
        {
            emit(new TACCall(false, dest, mangle(node->symbol->name)));
        }
        else /* node->kind == NullaryNode::CLOSURE */
        {
            // If the function is not completely applied, then this nullary node
            // evaluates to a function type -- create a closure
            size_t size = sizeof(SplObject) + 8;
            emit(new TACCall(true, dest, "gcAllocate", {std::make_shared<ConstAddress>(size)}));

            // SplObject header fields
            emit(new TACLeftIndexedAssignment(dest, offsetof(SplObject, constructorTag), ConstAddress::UnboxedZero));

            emit(new TACLeftIndexedAssignment(
                dest,
                offsetof(SplObject, sizeInWords),
                ConstAddress::UnboxedZero));

            // Address of the function as an unboxed member
            emit(new TACLeftIndexedAssignment(dest, sizeof(SplObject), getNameAddress(node->symbol)));
        }
    }
}

void TACCodeGen::visit(IntNode* node)
{
    node->address.reset(new ConstAddress(2 * node->value + 1));
}

void TACCodeGen::visit(BoolNode* node)
{
    if (node->value)
    {
        node->address = ConstAddress::True;
    }
    else
    {
        node->address = ConstAddress::False;
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
        node->address = node->children.back()->address;
    }
}

void TACCodeGen::visit(IfNode* node)
{
    TACLabel* trueBranch  = new TACLabel;
    TACLabel* falseBranch = new TACLabel;

    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, falseBranch);

    emit(trueBranch);

    node->body->accept(this);

    emit(falseBranch);
}

void TACCodeGen::visit(IfElseNode* node)
{
    TACLabel* trueBranch = new TACLabel;
    TACLabel* falseBranch = new TACLabel;
    TACLabel* endLabel = new TACLabel;

    if (!node->address) node->address = makeTemp();

    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, falseBranch);

    emit(trueBranch);
    std::shared_ptr<Address> bodyValue = visitAndGet(*node->body);
    if (bodyValue) emit(new TACAssign(node->address, bodyValue));
    emit(new TACJump(endLabel));

    emit(falseBranch);
    std::shared_ptr<Address> elseValue = visitAndGet(*node->else_body);
    if (elseValue) emit(new TACAssign(node->address, elseValue));

    emit(endLabel);
}

void TACCodeGen::visit(WhileNode* node)
{
    TACLabel* beginLabel = new TACLabel;
    TACLabel* endLabel = new TACLabel;

    emit(beginLabel);

    TACLabel* trueBranch = new TACLabel;
    _conditionalCodeGen.visitCondition(*node->condition, trueBranch, endLabel);

    emit(trueBranch);

    // Push a new inner loop on the (implicit) stack
    TACLabel* prevLoopEnd = _currentLoopEnd;
    _currentLoopEnd = endLabel;

    node->body->accept(this);

    _currentLoopEnd = prevLoopEnd;

    emit(new TACJump(beginLabel));

    emit(endLabel);
}

void TACCodeGen::visit(ForeverNode* node)
{
    TACLabel* beginLabel = new TACLabel;
    TACLabel* endLabel = new TACLabel;

    emit(beginLabel);

    // Push a new inner loop on the (implicit) stack
    TACLabel* prevLoopEnd = _currentLoopEnd;
    _currentLoopEnd = endLabel;

    node->body->accept(this);

    _currentLoopEnd = prevLoopEnd;

    emit(new TACJump(beginLabel));

    emit(endLabel);
}

void TACCodeGen::visit(BreakNode* node)
{
    emit(new TACJump(_currentLoopEnd));
}

void TACCodeGen::visit(AssignNode* node)
{
    std::shared_ptr<Address> dest = getNameAddress(node->symbol);

    if (node->symbol->type->isBoxed())
    {
        std::shared_ptr<Address> value = visitAndGet(*node->value);
        emit(new TACAssign(dest, value));
    }
    else
    {
        // Try to make the rhs of this assignment construct its result directly
        // in the right spot
        node->value->address = getNameAddress(node->symbol);
        std::shared_ptr<Address> value = visitAndGet(*node->value);

        // If it was stored somewhere else (for example, if no computation was
        // done, in a statement like x = y), then actually do the assignment
        if (value != getNameAddress(node->symbol))
        {
            emit(new TACAssign(dest, value));
        }
    }
}

void TACCodeGen::visit(LetNode* node)
{
    std::shared_ptr<Address> dest = getNameAddress(node->symbol);

    if (node->symbol->type->isBoxed())
    {
        std::shared_ptr<Address> value = visitAndGet(*node->value);
        emit(new TACAssign(dest, value));
    }
    else
    {
        // Try to make the rhs of this assignment construct its result directly
        // in the right spot
        node->value->address = getNameAddress(node->symbol);
        std::shared_ptr<Address> value = visitAndGet(*node->value);

        // If it was stored somewhere else (for example, if no computation was
        // done, in a statement like x = y), then actually do the assignment
        if (value != getNameAddress(node->symbol))
        {
            emit(new TACAssign(dest, value));
        }
    }
}

void TACCodeGen::visit(MatchNode* node)
{
    std::shared_ptr<Address> body = visitAndGet(*node->body);
    ValueConstructor* constructor = node->valueConstructor;

    // Copy over each of the members of the constructor pattern
    for (size_t i = 0; i < node->symbols.size(); ++i)
    {
        const Symbol* member = node->symbols.at(i);
        size_t location = constructor->members().at(i).location;

        emit(new TACRightIndexedAssignment(getNameAddress(member), body, sizeof(SplObject) + 8 * location));
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
            std::shared_ptr<Address> arg = visitAndGet(*node->arguments[0]);

            emit(new TACConditionalJump(arg, "==", ConstAddress::UnboxedZero, _trueBranch));
            emit(new TACJump(_falseBranch));
            return;
        }
    }

    wrapper(*node);
}

void TACCodeGen::visit(FunctionCallNode* node)
{
    std::vector<std::shared_ptr<Address>> arguments;
    for (auto& i : node->arguments)
    {
        i->accept(this);
        arguments.push_back(i->address);
    }

    if (!node->address)
    {
        if (unwrap(node->type) != Type::Unit)
            node->address = makeTemp();
    }
    else
    {
        assert(unwrap(node->type) != Type::Unit);
    }

    std::shared_ptr<Address> result = node->address;

    if (node->symbol->kind == kFunction && node->symbol->asFunction()->isBuiltin)
    {
        if (node->target == "not")
        {
            assert(arguments.size() == 1);

            TACLabel* endLabel = new TACLabel;
            TACLabel* trueBranch = new TACLabel;

            emit(new TACJumpIf(arguments[0], trueBranch));
            emit(new TACAssign(result, ConstAddress::True));
            emit(new TACJump(endLabel));
            emit(trueBranch);
            emit(new TACAssign(result, ConstAddress::False));
            emit(endLabel);
        }
        else if (node->target == "+")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::BADD, arguments[1]));
        }
        else if (node->target == "-")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::BSUB, arguments[1]));
        }
        else if (node->target == "*")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::BMUL, arguments[1]));
        }
        else if (node->target == "/")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::BDIV, arguments[1]));
        }
        else if (node->target == "%")
        {
            assert(arguments.size() == 2);
            emit(new TACBinaryOperation(result, arguments[0], BinaryOperation::BMOD, arguments[1]));
        }
        else
        {
            assert(false);
        }
    }
    else if (node->symbol->kind == kFunction)
    {
        std::string name;
        bool external = node->symbol->asFunction()->isExternal;
        if (external)
        {
            name = FOREIGN_NAME(node->symbol->name);
        }
        else
        {
            name = mangle(node->symbol->name);
        }

        bool foreign = node->symbol->asFunction()->isForeign;
        emit(new TACCall(foreign, result, name, arguments, external));
    }
    else /* node->symbol->kind == kVariable */
    {
        // The variable represents a closure, so extract the actual function
        // address
        std::shared_ptr<Address> functionAddress = makeTemp();
        emit(new TACRightIndexedAssignment(functionAddress, getNameAddress(node->symbol), sizeof(SplObject)));
        emit(new TACIndirectCall(result, functionAddress, arguments));
    }
}

void TACCodeGen::visit(ReturnNode* node)
{
    std::shared_ptr<Address> result = visitAndGet(*node->expression);
    emit(new TACAssign(_currentFunction->returnValue, result));
    emit(new TACJump(_functionEnd));
}

void TACCodeGen::visit(VariableNode* node)
{
    assert(node->symbol->kind == kVariable);
    node->address = getNameAddress(node->symbol);
}

void TACCodeGen::visit(MemberAccessNode* node)
{
    std::shared_ptr<Address> varAddress(getNameAddress(node->varSymbol));

    if (!node->address)
        node->address = makeTemp();

    std::shared_ptr<Address> result = node->address;

    emit(new TACRightIndexedAssignment(result, varAddress, sizeof(SplObject) + 8 * node->memberLocation));
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
    std::vector<TACLabel*> caseLabels;
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        caseLabels.push_back(new TACLabel);
    }
    TACLabel* endLabel = new TACLabel;

    std::shared_ptr<Address> expr = visitAndGet(*node->expr);

    // Get constructor tag of expr
    std::shared_ptr<Address> tag = makeTemp();
    emit(new TACRightIndexedAssignment(tag, expr, offsetof(SplObject, constructorTag)));

    // Jump to the appropriate case based on the tag
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        auto& arm = node->arms[i];
        size_t armTag = arm->constructorTag;
        emit(new TACConditionalJump(tag, "==", std::make_shared<ConstAddress>(armTag), caseLabels[i]));
    }

    // If nothing matches
    emit(new TACJump(endLabel));

    // Individual arms
    for (size_t i = 0; i < node->arms.size(); ++i)
    {
        auto& arm = node->arms[i];

        emit(caseLabels[i]);

        // The arm needs to know the expression so that it can extract fields
        arm->address = expr;
        arm->accept(this);

        emit(new TACJump(endLabel));
    }

    emit(endLabel);
}

void TACCodeGen::visit(MatchArm* node)
{
    ValueConstructor* constructor = node->valueConstructor;

    // Copy over each of the members of the constructor pattern
    for (size_t i = 0; i < node->symbols.size(); ++i)
    {
        const Symbol* member = node->symbols.at(i);
        size_t location = constructor->members().at(i).location;

        emit(new TACRightIndexedAssignment(getNameAddress(member), node->address, sizeof(SplObject) + 8 * location));
    }

    node->body->accept(this);
}

void TACCodeGen::visit(StringLiteralNode* node)
{
    node->address = getNameAddress(node->symbol);
    _tacProgram.staticStrings.emplace_back(node->address, node->content);
}

void TACCodeGen::createConstructor(ValueConstructor* constructor, size_t constructorTag)
{
    const std::vector<ValueConstructor::MemberDesc> members = constructor->members();

    // For now, every member takes up exactly 8 bytes (either directly or as a pointer).
    size_t size = sizeof(SplObject) + 8 * members.size();

    // Allocate room for the object
    _currentFunction->returnValue = makeTemp();
    emit(new TACCall(
        true,
        _currentFunction->returnValue,
        "gcAllocate",
        {std::make_shared<ConstAddress>(size)}));

    //// Fill in the members with the constructor arguments

    // SplObject header fields
    emit(new TACLeftIndexedAssignment(_currentFunction->returnValue, offsetof(SplObject, constructorTag), std::make_shared<ConstAddress>(constructorTag)));
    emit(new TACLeftIndexedAssignment(_currentFunction->returnValue, offsetof(SplObject, sizeInWords), std::make_shared<ConstAddress>(members.size())));

    // Individual members
    for (size_t i = 0; i < members.size(); ++i)
    {
        auto& member = members[i];
        size_t location = member.location;

        std::shared_ptr<Address> param(new NameAddress(member.name, NameTag::Local));
        _currentFunction->regParams.push_back(param);
        _currentFunction->locals.push_back(param);

        emit(new TACLeftIndexedAssignment(_currentFunction->returnValue, sizeof(SplObject) + 8 * location, param));
    }
}
