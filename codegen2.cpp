#include "codegen2.hpp"

#include <iostream>

void CodeGen2::visit(AssignNode* node)
{
    std::cout << "x = y + z" << std::endl;
}
