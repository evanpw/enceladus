#ifndef ADDRESS_HPP
#define ADDRESS_HPP

#include <memory>
#include <string>

#include "symbol.hpp"

// These structs represent the different kinds of operands in the three-address
// code used as an intermediate representation

struct NameAddress;
struct ConstAddress;
struct TempAddress;

enum class AddressTag {Name, Const, Temp};

struct Address
{
    Address(AddressTag tag) : tag(tag) {}

    NameAddress& asName();
    ConstAddress& asConst();
    TempAddress& asTemp();

    virtual std::string str() const = 0;

    AddressTag tag;
};

enum class NameTag {Local, Global, Param, Function};

struct NameAddress : public Address
{
    NameAddress(const Symbol* symbol);
    NameAddress(const std::string& name, NameTag nameTag);

    virtual std::string str() const override;

    std::string name;
    NameTag nameTag;
};

struct ConstAddress : public Address
{
    ConstAddress(long value)
    : Address(AddressTag::Const)
    , value(value)
    {}

    virtual std::string str() const override;

    long value;

    static std::shared_ptr<ConstAddress> True;
    static std::shared_ptr<ConstAddress> False;
    static std::shared_ptr<ConstAddress> Zero;
    static std::shared_ptr<ConstAddress> One;
};

struct TempAddress : public Address
{
    TempAddress(long number)
    : Address(AddressTag::Temp)
    , number(number)
    {}

    virtual std::string str() const override;

    long number;
};

#endif
