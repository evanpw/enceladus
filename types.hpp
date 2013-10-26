#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>

class Type
{
public:
    const std::string& name() const { return name_; }

    static const Type Void;
    static const Type Int;
    static const Type Bool;
    static const Type List;

    static const Type* lookup(const std::string& name);

private:
    Type(const char* name) : name_(name) {}
    Type(const Type& rhs) = delete;
    Type& operator=(const Type& rhs) = delete;

    std::string name_;
};

#endif
