#ifndef TO_SSA_HPP
#define TO_SSA_HPP

#include "ir/basic_block.hpp"
#include "ir/context.hpp"
#include "ir/function.hpp"
#include "ir/value.hpp"

#include <unordered_map>
#include <set>
#include <stack>
#include <vector>

// Got a lot of stuff from here: http://www.cs.utexas.edu/users/mckinley/380C/

struct PhiDescription
{
    PhiDescription(Value* original)
    : original(original)
    {}

    Value* original;

    Value* dest = nullptr;
    std::vector<std::pair<BasicBlock*, Value*>> sources;
};

typedef std::unordered_map<BasicBlock*, std::set<BasicBlock*>> Dominators;
typedef std::unordered_map<BasicBlock*, BasicBlock*> ImmDominators;
typedef std::unordered_map<BasicBlock*, std::vector<BasicBlock*>> DomFrontier;
typedef std::unordered_map<BasicBlock*, std::vector<PhiDescription>> PhiList;

class ToSSA
{
public:
    ToSSA(Function* function);
    void run();

private:
    Dominators findDominators();
    ImmDominators getImmediateDominators(const Dominators& dom);
    DomFrontier getDominanceFrontiers(const ImmDominators& idom);
    PhiList calculatePhiNodes(const DomFrontier& df);
    Value* generateName(Value* variable);
    void rename(BasicBlock* block, PhiList& phis);
    void insertPhis(PhiList& phis);
    void killDeadPhis();

    Function* _function;
    std::unordered_map<Value*, std::stack<Value*>> _phiStack;
    std::unordered_set<BasicBlock*> _visited;
    std::unordered_map<Value*, int> _counter;
};

#endif
