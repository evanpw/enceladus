#include "address.hpp"

NameAddress& Address::asName()
{
    return dynamic_cast<NameAddress&>(*this);
}

ConstAddress& Address::asConst()
{
    return dynamic_cast<ConstAddress&>(*this);
}

TempAddress& Address::asTemp()
{
    return dynamic_cast<TempAddress&>(*this);
}
