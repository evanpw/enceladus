#include <cstring>
#include "types.hpp"

template<class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

TypeTable::TypeTable()
{
    // Built-in types
    table_.emplace(std::make_pair("Void", make_unique<Type>("Void", true)));
    table_.emplace(std::make_pair("Int", make_unique<Type>("Int", true)));
    table_.emplace(std::make_pair("Bool", make_unique<Type>("Bool", true)));
    table_.emplace(std::make_pair("List", make_unique<Type>("List", false)));
    table_.emplace(std::make_pair("Tree", make_unique<Type>("Tree", false)));
}

void TypeTable::insert(const std::string& name, Type* type)
{
    table_.emplace(std::make_pair(name, std::unique_ptr<Type>(type)));
}

const Type* TypeTable::lookup(const std::string& name)
{
    auto i = table_.find(name);
    if (i == table_.end())
    {
        return nullptr;
    }
    else
    {
        return i->second.get();
    }
}

