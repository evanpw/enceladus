#include "semantic/types.hpp"

#include "ast/ast.hpp"
#include "lib/library.h"
#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

void Type::assign(Type* rhs)
{
    assert(isVariable());
    get<TypeVariable>()->assign(rhs);
}

std::string TypeVariable::str() const
{
    std::stringstream ss;

    if (!_name.empty())
    {
        ss << _name;
    }
    else
    {
        if (!_quantified)
            ss << "'";

        ss << "T" << _index;
    }

    if (!_constraints.empty())
    {
        ss << ": ";

        bool first = true;
        for (const Trait* constraint : _constraints)
        {
            if (!first) ss << " + ";
            ss << constraint->str();
            first = false;
        }
    }

    return ss.str();
}

void TypeVariable::assign(Type* rhs)
{
    assert(!_quantified);
    assert(!_references.empty());

    // Assume that if assigning to a concrete type that we've already checked
    // all of the constraints.

    // The type variable obtained by unifying two type variables inherits the
    // contraints of both
    if (rhs->isVariable())
    {
        TypeVariable* rhsVariable = rhs->get<TypeVariable>();
        for (const Trait* constraint : _constraints)
        {
            rhsVariable->addConstraint(constraint);
        }
    }

    // This is needed to keep myself alive until the end of this function,
    // because the for-loop below will reduce the reference count down to zero
    std::shared_ptr<TypeImpl> keepAlive = _references[0]->_impl;

    for (Type* reference : _references)
    {
        reference->_impl = rhs->_impl;
        rhs->_impl->addReference(reference);
    }

    _references.clear();
}

TypeTable::TypeTable()
{
    Int = createBaseType("Int", true, 64, true);
    UInt = createBaseType("UInt", true, 64, false);
    UInt8 = createBaseType("UInt8", true, 8, false);

    Num = createTrait("Num");
    Num->addInstance(Int);
    Num->addInstance(UInt);
    Num->addInstance(UInt8);

    Bool = createBaseType("Bool", true);
    Unit = createBaseType("Unit", true);
    String = createBaseType("String", false);

    Function = createConstructedType("Function", {createTypeVariable("T", true)});
    Array = createConstructedType("Array", {createTypeVariable("T", true)});
}

std::pair<size_t, ValueConstructor*> TypeImpl::getValueConstructor(const std::string& name) const
{
    for (size_t i = 0; i < _valueConstructors.size(); ++i)
    {
        if (_valueConstructors[i]->str() == name)
        {
            return std::make_pair(i, _valueConstructors[i]);
        }
    }

    return std::make_pair(0, nullptr);
}

std::string FunctionType::str() const
{
    std::stringstream ss;

    if (_inputs.empty())
    {
        ss << "||";
    }
    else
    {
        ss << "|";

        for (size_t i = 0; i < _inputs.size(); ++i)
        {
            ss << _inputs[i]->str();

            if (i + 1 < _inputs.size())
                ss << ", ";
        }

        ss << "|";
    }

    ss << " -> " << _output->str();

    return ss.str();
}

ConstructedType::ConstructedType(TypeTable* table, const std::string& name, std::vector<Type*>&& typeParameters, const ConstructedType* prototype)
: TypeImpl(table, ttConstructed), _name(name), _typeParameters(typeParameters), _prototype(prototype)
{
    if (!_prototype)
    {
        _prototype = this;
    }
}

Type* ConstructedType::instantiate(std::vector<Type*>&& typeParameters) const
{
    // Can only instantiate prototypical types
    assert(_prototype == this);

    return _table->createConstructedType(_name, std::move(typeParameters), this);
}

std::string ConstructedType::str() const
{
    std::stringstream ss;

    if (_name == "List")
    {
        assert(_typeParameters.size() == 1);
        ss << "[" << _typeParameters[0]->str() << "]";
    }
    else
    {
        ss << _name;
        ss << "<";

        bool first = true;
        for (const Type* type : _typeParameters)
        {
            if (!first) ss << ", ";
            ss << type->str();
            first = false;
        }
        ss << ">";
    }

    return ss.str();
}

// Passed through the prototype
const std::vector<ValueConstructor*>& ConstructedType::valueConstructors() const
{
    if (_prototype == this)
    {
        return TypeImpl::valueConstructors();
    }
    else
    {
        return _prototype->valueConstructors();
    }
}

std::pair<size_t, ValueConstructor*> ConstructedType::getValueConstructor(const std::string& name) const
{
    if (_prototype == this)
    {
        return TypeImpl::getValueConstructor(name);
    }
    else
    {
        return _prototype->getValueConstructor(name);
    }
}

void ConstructedType::addValueConstructor(ValueConstructor* valueConstructor)
{
    assert(_prototype == this);
    TypeImpl::addValueConstructor(valueConstructor);
}

int TypeVariable::_count;

ValueConstructor::ValueConstructor(const std::string& name,
                                   size_t constructorTag,
                                   const std::vector<Type*>& memberTypes,
                                   const std::vector<std::string>& memberNames)
: _name(name)
, _constructorTag(constructorTag)
{
    assert(memberNames.empty() || (memberNames.size() == memberTypes.size()));

    for (size_t i = 0; i < memberTypes.size(); ++i)
    {
        std::string memberName = memberNames.empty() ? "" : memberNames[i];
        Type* type = memberTypes[i];
        _members.emplace_back(memberName, type);
    }
}
