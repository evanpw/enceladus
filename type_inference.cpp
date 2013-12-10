#include "semantic.hpp"
#include "simple.tab.h"
#include "utility.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>

void TypeChecker::inferenceError(AstNode* node, const std::string& msg)
{
    std::stringstream ss;

    ss << "Near line " << node->location()->first_line << ", "
       << "column " << node->location()->first_column << ": "
       << "error: " << msg;

    throw TypeInferenceError(ss.str());
}

std::set<TypeVariable*> TypeChecker::getFreeVars(Symbol* symbol)
{
    std::set<TypeVariable*> freeVars;
    freeVars += symbol->type->freeVars();

    if (symbol->kind == kFunction)
    {
        FunctionSymbol* functionSymbol = static_cast<FunctionSymbol*>(symbol);

        assert(functionSymbol->type->tag() == ttFunction);
        FunctionType* functionType = functionSymbol->type->type()->get<FunctionType>();
        for (auto& type : functionType->inputs())
        {
            freeVars += type->freeVars();
        }
        freeVars += functionType->output()->freeVars();
    }

    return freeVars;
}

std::unique_ptr<TypeScheme> TypeChecker::generalize(const std::shared_ptr<Type>& type)
{
    if (_verbose)
        std::cerr << "\tGeneralizing " << type->name() << std::endl;

    std::set<TypeVariable*> typeFreeVars = type->freeVars();

    std::set<TypeVariable*> envFreeVars;
    for (auto i = scopes_.rbegin(); i != scopes_.rend(); ++i)
    {
        Scope* scope = *i;
        for (auto& j : scope->symbols())
        {
            const std::unique_ptr<Symbol>& symbol = j.second;
            envFreeVars += getFreeVars(symbol.get());
        }
    }

    return make_unique<TypeScheme>(type, typeFreeVars - envFreeVars);
}

std::shared_ptr<Type> TypeChecker::instantiate(const std::shared_ptr<Type>& type, const std::map<TypeVariable*, std::shared_ptr<Type>>& replacements)
{
    switch (type->tag())
    {
        case ttBase:
            return type;

        case ttVariable:
        {
            TypeVariable* typeVariable = type->get<TypeVariable>();

            auto i = replacements.find(typeVariable);
            if (i != replacements.end())
            {
                return i->second;
            }
            else
            {
                return type;
            }
        }

        case ttFunction:
        {
            FunctionType* functionType = type->get<FunctionType>();

            std::vector<std::shared_ptr<Type>> newInputs;
            for (const std::shared_ptr<Type> input : functionType->inputs())
            {
                newInputs.push_back(instantiate(input, replacements));
            }

            return FunctionType::create(newInputs, instantiate(functionType->output(), replacements));
        }

        case ttConstructed:
        {
            std::vector<std::shared_ptr<Type>> params;

            ConstructedType* constructedType = type->get<ConstructedType>();
            for (const std::shared_ptr<Type>& parameter : constructedType->typeParameters())
            {
                params.push_back(instantiate(parameter, replacements));
            }

            return ConstructedType::create(constructedType->typeConstructor(), params);
        }
    }

    assert(false);
    return std::shared_ptr<Type>();
}

std::shared_ptr<Type> TypeChecker::instantiate(TypeScheme* scheme)
{
    std::map<TypeVariable*, std::shared_ptr<Type>> replacements;
    for (TypeVariable* boundVar : scheme->quantified())
    {
        replacements[boundVar] = TypeVariable::create(true);
    }

    std::shared_ptr<Type> resultType = instantiate(scheme->type(), replacements);

    if (_verbose)
        std::cerr << "\tInstantiating " << scheme->name() << " => " << resultType->name() << std::endl;

    return resultType;
}

bool TypeChecker::occurs(TypeVariable* variable, const std::shared_ptr<Type>& value)
{
    switch (value->tag())
    {
        case ttBase:
            return false;

        case ttVariable:
        {
            TypeVariable* typeVariable = value->get<TypeVariable>();
            return typeVariable == variable;
        }

        case ttFunction:
        {
            FunctionType* functionType = value->get<FunctionType>();

            for (auto& input : functionType->inputs())
            {
                if (occurs(variable, input)) return true;
            }

            return occurs(variable, functionType->output());
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = value->get<ConstructedType>();
            for (const std::shared_ptr<Type>& parameter : constructedType->typeParameters())
            {
                if (occurs(variable, parameter)) return true;
            }

            return false;
        }
    }

    assert(false);
}

void TypeChecker::bindVariable(const std::shared_ptr<Type>& variable, const std::shared_ptr<Type>& value, AstNode* node)
{
    assert(variable->tag() == ttVariable);

    // Check to see if the value is actually the same type variable, and don't
    // rebind
    if (value->tag() == ttVariable)
    {
        if (variable->get<TypeVariable>() == value->get<TypeVariable>()) return;
    }

    // Make sure that the variable actually occurs in the type
    if (occurs(variable->get<TypeVariable>(), value))
    {
        std::stringstream ss;
        ss << "variable " << variable->name() << " already occurs in " << value->name();
        inferenceError(node, ss.str());
    }

    // Polymorphic type variables can be bound only to lifted (i.e., boxed) types
    TypeVariable* typeVariable = variable->get<TypeVariable>();
    if (typeVariable->isPolymorphic() && !value->isBoxed())
    {
        std::stringstream ss;
        ss << "type variable " << variable->name() << " cannot be bound to unboxed type " << value->name();
        //inferenceError(node, ss.str());
    }

    if (_verbose)
        std::cerr << "\tBinding " << variable->name() << " = " << value->name() << std::endl;

    // And if these check out, make the substitution
    *variable = *value;
}

void TypeChecker::unify(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs, AstNode* node)
{
    if (_verbose)
        std::cerr << "\tUnifying " << lhs->name() << " and " << rhs->name() << std::endl;

    // Two base types can be unified only if equal (we don't have inheritance)
    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        if (lhs->name() != rhs->name())
        {
            std::stringstream ss;
            ss << "cannot unify base types " << lhs->name() << " and " << rhs->name();
            inferenceError(node, ss.str());
        }
    }
    else if (lhs->tag() == ttVariable)
    {
        bindVariable(lhs, rhs, node);
    }
    else if (rhs->tag() == ttVariable)
    {
        bindVariable(rhs, lhs, node);
    }
    else if (lhs->tag() == ttFunction && rhs->tag() == ttFunction)
    {
        FunctionType* lhsFunction = lhs->get<FunctionType>();
        FunctionType* rhsFunction = rhs->get<FunctionType>();

        if (lhsFunction->inputs().size() == rhsFunction->inputs().size())
        {
            for (size_t i = 0; i < lhsFunction->inputs().size(); ++i)
            {
                unify(lhsFunction->inputs().at(i), rhsFunction->inputs().at(i), node);
            }

            unify(lhsFunction->output(), rhsFunction->output(), node);

            return;
        }

        std::stringstream ss;
        ss << "Cannot unify function types " << lhs->name() << " and " << rhs->name();
        inferenceError(node, ss.str());
    }
    else if (lhs->tag() == ttConstructed && rhs->tag() == ttConstructed)
    {
        ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
        ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

        if (lhsConstructed->typeConstructor() != rhsConstructed->typeConstructor())
        {
            std::stringstream ss;
            ss << "cannot unify constructed types " << lhs->name() << " and " << rhs->name();
            inferenceError(node, ss.str());
        }

        assert(lhsConstructed->typeParameters().size() == rhsConstructed->typeParameters().size());

        for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
        {
            unify(lhsConstructed->typeParameters().at(i), rhsConstructed->typeParameters().at(i), node);
        }
    }
    else
    {
        // Can't be unified
        std::stringstream ss;
        ss << "cannot unify types " << lhs->name() << " and " << rhs->name();
        inferenceError(node, ss.str());
    }
}

// Internal nodes
void TypeChecker::visit(ProgramNode* node)
{
    typeTable_ = node->typeTable();

    for (auto& child : node->children())
    {
        child->accept(this);
        unify(child->type(), typeTable_->getBaseType("Unit"), node);
    }

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(ComparisonNode* node)
{
    node->lhs()->accept(this);
    unify(node->lhs()->type(), typeTable_->getBaseType("Int"), node);

    node->rhs()->accept(this);
    unify(node->rhs()->type(), typeTable_->getBaseType("Int"), node);

    node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(BinaryOperatorNode* node)
{
    node->lhs()->accept(this);
    unify(node->lhs()->type(), typeTable_->getBaseType("Int"), node);

    node->rhs()->accept(this);
    unify(node->rhs()->type(), typeTable_->getBaseType("Int"), node);

    node->setType(typeTable_->getBaseType("Int"));
}

void TypeChecker::visit(NullNode* node)
{
    node->child()->accept(this);

    // TODO: If this was a real function call, we wouldn't need this special
    // check here. It would happen when we bound the type variable to the type
    // of the argument.
    if (!node->child()->type()->isBoxed())
    {
        std::stringstream msg;
        msg << "cannot call null on unboxed type " << node->child()->type()->name();
        semanticError(node, msg.str());
    }

    node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(LogicalNode* node)
{
    node->lhs()->accept(this);
    unify(node->lhs()->type(), typeTable_->getBaseType("Bool"), node);

    node->rhs()->accept(this);
    unify(node->rhs()->type(), typeTable_->getBaseType("Bool"), node);

    node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(MatchNode* node)
{
    node->body()->accept(this);

    assert(node->constructorSymbol()->type->quantified().empty());
    assert(node->constructorSymbol()->type->tag() == ttFunction);

    FunctionType* functionType = node->constructorSymbol()->type->type()->get<FunctionType>();
    unify(node->body()->type(), functionType->output(), node);

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(BlockNode* node)
{
    for (auto& child : node->children())
    {
        child->accept(this);
        unify(child->type(), typeTable_->getBaseType("Unit"), node);
    }

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(IfNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), typeTable_->getBaseType("Bool"), node);

    node->body()->accept(this);
    unify(node->body()->type(), typeTable_->getBaseType("Unit"), node);

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(IfElseNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), typeTable_->getBaseType("Bool"), node);

    node->body()->accept(this);
    unify(node->body()->type(), typeTable_->getBaseType("Unit"), node);

    node->else_body()->accept(this);
    unify(node->else_body()->type(), typeTable_->getBaseType("Unit"), node);

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(WhileNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), typeTable_->getBaseType("Bool"), node);

    node->body()->accept(this);
    unify(node->body()->type(), typeTable_->getBaseType("Unit"), node);

    node->setType(typeTable_->getBaseType("Unit"));
}

// TODO: Look at this more closely
void TypeChecker::visit(AssignNode* node)
{
    node->value()->accept(this);

    assert(node->symbol()->type->quantified().empty());
    unify(node->value()->type(), node->symbol()->type->type(), node);

    node->setType(typeTable_->getBaseType("Unit"));
}

// Leaf nodes
void TypeChecker::visit(NullaryNode* node)
{
    if (node->symbol()->kind == kVariable)
    {
        assert(node->symbol()->type->quantified().empty());
        node->setType(node->symbol()->type->type());
    }
    else
    {
        assert(node->symbol()->kind == kFunction);

        std::shared_ptr<Type> returnType = TypeVariable::create();
        std::shared_ptr<Type> functionType = instantiate(node->symbol()->type.get());
        unify(functionType, FunctionType::create({}, returnType), node);

        node->setType(returnType);
    }
}

void TypeChecker::visit(IntNode* node)
{
    node->setType(typeTable_->getBaseType("Int"));
}

void TypeChecker::visit(BoolNode* node)
{
    node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(NilNode* node)
{
    // TODO: Turn this into a real function call

    std::shared_ptr<Type> polyList = ConstructedType::create(
        typeTable_->getTypeConstructor("List"),
        {TypeVariable::create()});

    node->setType(polyList);
}

void TypeChecker::visit(FunctionCallNode* node)
{
    std::vector<std::shared_ptr<Type>> paramTypes;
    for (size_t i = 0; i < node->arguments().size(); ++i)
    {
        AstNode* argument = node->arguments().at(i).get();
        argument->accept(this);

        paramTypes.push_back(argument->type());
    }

    std::shared_ptr<Type> returnType = TypeVariable::create();
    std::shared_ptr<Type> functionType = instantiate(node->symbol()->type.get());

    unify(functionType, FunctionType::create(paramTypes, returnType), node);

    node->setType(returnType);
}

void TypeChecker::visit(ReturnNode* node)
{
    // This isn't really type checking, but this is the easiest place to put this check.
    if (_enclosingFunction == nullptr)
    {
        std::stringstream msg;
        msg << "Cannot return from top level.";
        semanticError(node, msg.str());

        return;
    }

    node->expression()->accept(this);

    assert(_enclosingFunction->symbol()->type->quantified().empty());
    assert(_enclosingFunction->symbol()->type->tag() == ttFunction);

    // Value of expression must equal the return type of the enclosing function.
    FunctionType* functionType = _enclosingFunction->symbol()->type->type()->get<FunctionType>();
    unify(node->expression()->type(), functionType->output(), node);

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(FunctionDefNode* node)
{
    enterScope(node->scope());
    _enclosingFunction = node;

    // Recurse
    node->body()->accept(this);

    _enclosingFunction = nullptr;
    exitScope();

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(LetNode* node)
{
    node->value()->accept(this);

    assert(node->symbol()->type->quantified().empty());
    unify(node->value()->type(), node->symbol()->type->type(), node);

    node->setType(typeTable_->getBaseType("Unit"));
}
