#include "tac_instruction.hpp"

Label::Label()
: number(labelCount++)
{
}

std::string Label::str() const
{
    std::stringstream ss;
    ss << "__" << number;
    return ss.str();
}

long Label::labelCount = 0;
