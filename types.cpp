#include <cstring>
#include "types.hpp"

const Type Type::Void("Void", true);
const Type Type::Int("Int", true);
const Type Type::Bool("Bool", true);
const Type Type::List("List", false);
const Type Type::Tree("Tree", false);

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
    else if (name == std::string("Tree"))
    {
        return &Type::Tree;
    }
    else if (name == std::string("Void"))
    {
        return &Type::Void;
    }

    return nullptr;
}
