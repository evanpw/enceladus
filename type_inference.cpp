#include "semantic.hpp"
#include "simple.tab.h"
#include "utility.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>

void TypeInference::inferenceError(AstNode* node, const std::string& msg)
{
    std::stringstream ss;

    ss << "Near line " << node->location()->first_line << ", "
       << "column " << node->location()->first_column << ": "
       << "error: " << msg;

    throw TypeInferenceError(ss.str());
}

std::set<TypeVariable*> TypeInference::getFreeVars(Symbol* symbol)
{
    std::set<TypeVariable*> freeVars;
    freeVars += symbol->typeScheme->freeVars();

    if (symbol->kind == kFunction)
    {
        FunctionSymbol* functionSymbol = static_cast<FunctionSymbol*>(symbol);

        assert(functionSymbol->typeScheme->tag() == ttFunction);
        FunctionType* functionType = functionSymbol->typeScheme->type()->get<FunctionType>();
        for (auto& type : functionType->inputs())
        {
            freeVars += type->freeVars();
        }
        freeVars += functionType->output()->freeVars();
    }

    return freeVars;
}

std::unique_ptr<TypeScheme> TypeInference::generalize(const std::shared_ptr<Type>& type, const std::vector<Scope*>& scopes)
{
    std::set<TypeVariable*> typeFreeVars = type->freeVars();

    std::set<TypeVariable*> envFreeVars;
    for (auto i = scopes.rbegin(); i != scopes.rend(); ++i)
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

std::shared_ptr<Type> TypeInference::instantiate(const std::shared_ptr<Type>& type, const std::map<TypeVariable*, std::shared_ptr<Type>>& replacements)
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

std::shared_ptr<Type> TypeInference::instantiate(TypeScheme* scheme)
{
    std::map<TypeVariable*, std::shared_ptr<Type>> replacements;
    for (TypeVariable* boundVar : scheme->quantified())
    {
        replacements[boundVar] = TypeVariable::create(true);
    }

    return instantiate(scheme->type(), replacements);
}

bool TypeInference::occurs(TypeVariable* variable, const std::shared_ptr<Type>& value)
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

void TypeInference::bindVariable(const std::shared_ptr<Type>& variable, const std::shared_ptr<Type>& value, AstNode* node)
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
        inferenceError(node, ss.str());
    }

    // And if these check out, make the substitution
    *variable = *value;
}

void TypeInference::unify(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs, AstNode* node)
{
    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        // Two base types can be unified only if equal (we don't have inheritance)
        if (lhs->name() == rhs->name())
            return;
    }
    else if (lhs->tag() == ttVariable)
    {
        bindVariable(lhs, rhs, node);
        return;
    }
    else if (rhs->tag() == ttVariable)
    {
        bindVariable(rhs, lhs, node);
        return;
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
    }
    else if (lhs->tag() == ttConstructed && rhs->tag() == ttConstructed)
    {
        ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
        ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

        if (lhsConstructed->typeConstructor() == rhsConstructed->typeConstructor())
        {
            assert(lhsConstructed->typeParameters().size() == rhsConstructed->typeParameters().size());

            for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
            {
                unify(lhsConstructed->typeParameters().at(i), rhsConstructed->typeParameters().at(i), node);
            }

            return;
        }
    }

    // Can't be unified
    std::stringstream ss;
    ss << "cannot unify types " << lhs->name() << " and " << rhs->name();
    inferenceError(node, ss.str());
}
