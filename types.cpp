#include <cstring>
#include "types.hpp"

const Type Type::Void("Void");
const Type Type::Int("Int");
const Type Type::Bool("Bool");
const Type Type::List("List");

const Type* Type::lookup(const char* name)
{
    if (strcmp(name, "Int") == 0)
    {
        return &Type::Int;
    }
    else if (strcmp(name, "Bool") == 0)
    {
        return &Type::Bool;
    }
    else if (strcmp(name, "List") == 0)
    {
        return &Type::List;
    }

    return nullptr;
}
