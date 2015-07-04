#ifndef BASIC_BLOCK_HPP
#define BASIC_BLOCK_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "value.hpp"

struct Instruction;

struct BasicBlock : public Value
{
    virtual ~BasicBlock();

    virtual std::string str() const;
    void prepend(Instruction* inst);
    void append(Instruction* inst);

    void addPredecessor(BasicBlock* block)
    {
        _predecessors.push_back(block);
    }

    const std::vector<BasicBlock*>& predecessors() const
    {
        return _predecessors;
    }

    const std::vector<BasicBlock*>& successors() const
    {
        return _successors;
    }

    // Does this basic block end in a terminator instruction?
    bool isTerminated();

    // Basic block owns its instructions
    Instruction* first = nullptr;
    Instruction* last = nullptr;

private:
    friend class TACContext;
    BasicBlock(TACContext* context, int64_t seqNumber);

    static bool getTargets(Instruction* inst, std::vector<BasicBlock*>& targets);

    std::vector<BasicBlock*> _predecessors;
    std::vector<BasicBlock*> _successors;
};

#endif
