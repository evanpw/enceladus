#ifndef BASIC_BLOCK_HPP
#define BASIC_BLOCK_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "value.hpp"

struct Instruction;

struct BasicBlock : public Value
{
    BasicBlock(int64_t seqNumber);
    virtual ~BasicBlock();

    virtual std::string str() const;
    void append(Instruction* inst);

    std::vector<BasicBlock*> getSuccessors();

    Instruction* first = nullptr;
    Instruction* last = nullptr;

    int64_t seqNumber;
};

#endif
