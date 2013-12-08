#include "semantic.hpp"
#include "utility.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>

std::set<TypeVariable*> TypeChecker::getFreeVars(Symbol* symbol)
{
    std::set<TypeVariable*> freeVars;
    freeVars += symbol->type->freeVars();

    if (symbol->kind == kFunction)
    {
        FunctionSymbol* functionSymbol = static_cast<FunctionSymbol*>(symbol);
        for (auto& type : functionSymbol->paramTypes)
        {
            freeVars += type->freeVars();
        }
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

            return FunctionType::create(instantiate(functionType->domain(), replacements),
                                        instantiate(functionType->range(), replacements));
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
        replacements[boundVar] = TypeVariable::create();
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
            return occurs(variable, functionType->domain()) ||
                   occurs(variable, functionType->range());
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

void TypeChecker::bindVariable(const std::shared_ptr<Type>& variable, const std::shared_ptr<Type>& value)
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
        throw TypeInferenceError(ss.str());
    }

    if (_verbose)
        std::cerr << "\tBinding " << variable->name() << " = " << value->name() << std::endl;

    // And if these check out, make the substitution
    *variable = *value;
}

void TypeChecker::unify(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs)
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
            throw TypeInferenceError(ss.str());
        }
    }
    else if (lhs->tag() == ttVariable)
    {
        bindVariable(lhs, rhs);
    }
    else if (rhs->tag() == ttVariable)
    {
        bindVariable(rhs, lhs);
    }
    else if (lhs->tag() == ttFunction && rhs->tag() == ttFunction)
    {
        FunctionType* lhsFunction = lhs->get<FunctionType>();
        FunctionType* rhsFunction = rhs->get<FunctionType>();

        unify(lhsFunction->domain(), rhsFunction->domain());
        unify(lhsFunction->range(), rhsFunction->range());
    }
    else if (lhs->tag() == ttConstructed && rhs->tag() == ttConstructed)
    {
        ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
        ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

        if (lhsConstructed->typeConstructor() != rhsConstructed->typeConstructor())
        {
            std::stringstream ss;
            ss << "cannot unify constructed types " << lhs->name() << " and " << rhs->name();
            throw TypeInferenceError(ss.str());
        }

        assert(lhsConstructed->typeParameters().size() == rhsConstructed->typeParameters().size());

        for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
        {
            unify(lhsConstructed->typeParameters().at(i), rhsConstructed->typeParameters().at(i));
        }
    }
    else
    {
        // Can't be unified
        std::stringstream ss;
        ss << "cannot unify types " << lhs->name() << " and " << rhs->name();
        throw TypeInferenceError(ss.str());
    }
}

// Internal nodes
void TypeChecker::visit(ProgramNode* node)
{
    typeTable_ = node->typeTable();

    for (auto& child : node->children())
    {
        child->accept(this);
        unify(child->type(), typeTable_->getBaseType("Unit"));
    }

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(ComparisonNode* node)
{
    node->lhs()->accept(this);
    unify(node->lhs()->type(), typeTable_->getBaseType("Int"));

    node->rhs()->accept(this);
    unify(node->rhs()->type(), typeTable_->getBaseType("Int"));

    node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(BinaryOperatorNode* node)
{
    node->lhs()->accept(this);
    unify(node->lhs()->type(), typeTable_->getBaseType("Int"));

    node->rhs()->accept(this);
    unify(node->rhs()->type(), typeTable_->getBaseType("Int"));

    node->setType(typeTable_->getBaseType("Int"));
}

void TypeChecker::visit(NullNode* node)
{
    node->child()->accept(this);

    // FIXME: This should probably be done in a better way
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
    unify(node->lhs()->type(), typeTable_->getBaseType("Bool"));

    node->rhs()->accept(this);
    unify(node->rhs()->type(), typeTable_->getBaseType("Bool"));

    node->setType(typeTable_->getBaseType("Bool"));
}

void TypeChecker::visit(MatchNode* node)
{
    node->body()->accept(this);

    assert(node->constructorSymbol()->type->quantified().empty());
    unify(node->body()->type(), node->constructorSymbol()->type->type());

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(BlockNode* node)
{
    for (auto& child : node->children())
    {
        child->accept(this);
        unify(child->type(), typeTable_->getBaseType("Unit"));
    }

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(IfNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), typeTable_->getBaseType("Bool"));

    node->body()->accept(this);

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(IfElseNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), typeTable_->getBaseType("Bool"));

    node->body()->accept(this);
    node->else_body()->accept(this);

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(WhileNode* node)
{
    node->condition()->accept(this);
    unify(node->condition()->type(), typeTable_->getBaseType("Bool"));

    node->body()->accept(this);

    node->setType(typeTable_->getBaseType("Unit"));
}

void TypeChecker::visit(AssignNode* node)
{
    node->value()->accept(this);

    assert(node->symbol()->type->quantified().empty());
    unify(node->value()->type(), node->symbol()->type->type());

    node->setType(typeTable_->getBaseType("Unit"));
}

// Leaf nodes
void TypeChecker::visit(NullaryNode* node)
{
    assert(node->symbol()->type->quantified().empty());
    node->setType(node->symbol()->type->type());
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
    std::shared_ptr<Type> listOfInts = ConstructedType::create(
        typeTable_->getTypeConstructor("List"),
        {typeTable_->getBaseType("Int")});

    node->setType(listOfInts);
}

void TypeChecker::visit(FunctionCallNode* node)
{
    // The type of a function call is the return type of the function
    assert(node->symbol()->type->quantified().empty());
    node->setType(node->symbol()->type->type());

    assert(node->symbol()->paramTypes.size() == node->arguments().size());
    for (size_t i = 0; i < node->arguments().size(); ++i)
    {
        AstNode* argument = node->arguments().at(i).get();
        std::shared_ptr<TypeScheme> typeScheme = node->symbol()->paramTypes.at(i);

        argument->accept(this);

        assert(typeScheme->quantified().empty());
        unify(argument->type(), typeScheme->type());
    }
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

    // Value of expression must equal the return type of the enclosing function.
    assert(_enclosingFunction->symbol()->type->quantified().empty());
    unify(node->expression()->type(), _enclosingFunction->symbol()->type->type());

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

    // Baby type inference
    if (!node->typeName())
    {
        node->symbol()->type = TypeScheme::trivial(node->value()->type());
    }
    else
    {
        assert(node->symbol()->type->quantified().empty());
        unify(node->value()->type(), node->symbol()->type->type());
    }

    node->setType(typeTable_->getBaseType("Unit"));
}
