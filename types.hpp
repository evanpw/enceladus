#ifndef TYPES_HPP
#define TYPES_HPP

class Type
{
public:
    const char* name() const { return name_; }

    static const Type Void;
    static const Type Int;
    static const Type Bool;
    static const Type List;

    static const Type* lookup(const char* name);

private:
    Type(const char* name) : name_(name) {}
    Type(const Type& rhs) = delete;
    Type& operator=(const Type& rhs) = delete;

    const char* name_;
};

#endif
