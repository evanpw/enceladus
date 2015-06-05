#include "tac_instruction.hpp"

TACLabel::TACLabel()
: number(labelCount++)
{
}

long TACLabel::labelCount = 0;

const char* binaryOperationNames[] = {"b+", "b-", "b*", "b/", "b%", "u&", "u+", ">>"};
