#ifndef TYPES_HPP
#define TYPES_HPP

#include <memory>
#include <string>
#include <unordered_map>

class Type
{
public:
    Type(const char* name, bool isSimple) : name_(name), isSimple_(isSimple), constructorCount_(1) {}

    const std::string& name() const { return name_; }
    bool isSimple() const { return isSimple_; }
    int constructorCount() const { return constructorCount_; }

private:
    Type(const Type& rhs) = delete;
    Type& operator=(const Type& rhs) = delete;

    std::string name_;

    // True if the type fits into one 8-byte long, and is not heap-allocated
    bool isSimple_;

    int constructorCount_;
};

class TypeTable
{
public:
    TypeTable();

    const Type* lookup(const std::string& name);
    void insert(const std::string& name, Type* type);

private:
    std::unordered_map<std::string, std::unique_ptr<Type>> table_;
};

#endif
