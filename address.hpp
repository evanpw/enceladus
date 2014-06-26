#ifndef ADDRESS_HPP
#define ADDRESS_HPP

// These structs represent the different kinds of operands in the three-address
// code used as an intermediate representation

struct NameAddress;
struct ConstAddress;
struct TempAddress;

enum class AddressTag {Name, Const, Temp};

struct Address
{
    Address(AddressTag tag) : tag(tag) {}

    virtual ~Address() {}

    NameAddress& asName();
    ConstAddress& asConst();
    TempAddress& asTemp();

    AddressTag tag;
};

struct NameAddress : public Address
{
    NameAddress() : Address(AddressTag::Name) {}
};

struct ConstAddress : public Address
{
    ConstAddress() : Address(AddressTag::Const) {}
};

struct TempAddress : public Address
{
    TempAddress() : Address(AddressTag::Temp) {}
};

#endif
