#include <cstring>
#include "types.hpp"

const Type Type::Void("Void");
const Type Type::Int("Int");
const Type Type::Bool("Bool");
const Type Type::List("List");

const Type* Type::lookup(const std::string& name)
{
    if (name == std::string("Int"))
    {
        return &Type::Int;
    }
    else if (name == std::string("Bool"))
    {
        return &Type::Bool;
    }
    else if (name == std::string("List"))
    {
        return &Type::List;
    }

    return nullptr;
}
