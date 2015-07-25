#ifndef FROM_SSA_HPP
#define FROM_SSA_HPP

#include "function.hpp"

class FromSSA
{
public:
    FromSSA(Function* function);
    void run();

private:
    Function* _function;
};

#endif