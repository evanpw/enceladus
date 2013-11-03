#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>

class Type
{
public:
    const std::string& name() const { return name_; }
    bool isSimple() const { return isSimple_; }

    static const Type Void;
    static const Type Int;
    static const Type Bool;
    static const Type List;
    static const Type Tree;

    static const Type* lookup(const std::string& name);

private:
    Type(const char* name, bool isSimple) : name_(name), isSimple_(isSimple) {}
    Type(const Type& rhs) = delete;
    Type& operator=(const Type& rhs) = delete;

    std::string name_;

    // True if the type fits into one 8-byte long, and is not heap-allocated
    bool isSimple_;
};

#endif
