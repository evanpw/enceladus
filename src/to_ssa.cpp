#include "to_ssa.hpp"
#include <algorithm>

ToSSA::ToSSA(Function* function)
: _function(function)
{
}

void ToSSA::run()
{
    Dominators dom = findDominators();
    ImmDominators idom = getImmediateDominators(dom);
    DomFrontier df = getDominanceFrontiers(idom);
    PhiList allPhis = calculatePhiNodes(df);

    rename(_function->blocks[0], allPhis);
    insertPhis(allPhis);
    killDeadPhis();
}

// Compute the dominators of each basic block
Dominators ToSSA::findDominators()
{
    Dominators dom;
    std::vector<BasicBlock*> blocks = _function->blocks;
    BasicBlock* entry = blocks[0];

    // Only dominator of the entry block is itself
    dom[entry] = {entry};

    // The dominators of the rest of the blocks are a subset of the whole set
    for (size_t i = 1; i < blocks.size(); ++i)
    {
        dom[blocks[i]].insert(blocks.begin(), blocks.end());
    }

    // Simple N^2 algorithm based on the recursive definition of DOM
    bool changed;
    do
    {
        changed = false;

        for (size_t i = 1; i < blocks.size(); ++i)
        {
            BasicBlock* block = blocks[i];
            auto predecessors = block->predecessors();

            std::set<BasicBlock*> oldDom = dom[block];

            // The set of strict dominators of block is equal to the intersection
            // of the set of dominators of each of its predecessors
            std::set<BasicBlock*> newDom;
            if (!predecessors.empty())
            {
                newDom = dom[predecessors[0]];

                for (size_t i = 1; i < predecessors.size(); ++i)
                {
                    std::set<BasicBlock*> lhs = newDom;
                    const std::set<BasicBlock*>& rhs = dom[predecessors[i]];

                    newDom.clear();
                    std::set_intersection(
                        lhs.begin(), lhs.end(),
                        rhs.begin(), rhs.end(),
                        std::inserter(newDom, newDom.begin()));
                }
            }

            newDom.insert(block);

            if (oldDom != newDom)
            {
                changed = true;
                dom[block] = newDom;
            }
        }

    } while (changed);

    return dom;
}

ImmDominators ToSSA::getImmediateDominators(const Dominators& dom)
{
    ImmDominators idom;

    for (auto& item : dom)
    {
        BasicBlock* block = item.first;
        const std::set<BasicBlock*>& dominators = item.second;

        // A copy
        std::set<BasicBlock*> working = item.second;

        // IDOM must be a strict dominator
        working.erase(block);

        // Remove all elements which are strictly dominated by a dominator
        for (BasicBlock* d1 : dominators)
        {
            if (d1 == block) continue;

            const std::set<BasicBlock*>& dominators1 = dom.at(d1);

            for (BasicBlock* d2 : dominators)
            {
                if (d1 != d2 && dominators1.find(d2) != dominators1.end())
                {
                    working.erase(d2);
                }
            }
        }

        if (working.size() == 1)
        {
            idom[block] = *(working.begin());
        }
        else if (working.size() == 0)
        {
            // Should only happen for the entry block
            assert(block->predecessors().empty());
            idom[block] = nullptr;
        }
        else
        {
            assert(false);
        }
    }

    return idom;
}

DomFrontier ToSSA::getDominanceFrontiers(const ImmDominators& idom)
{
    DomFrontier df;

    for (auto& item : idom)
    {
        BasicBlock* block = item.first;
        BasicBlock* dominator = item.second;

        if (block->predecessors().size() < 2)
            continue;

        for (BasicBlock* predecessor : block->predecessors())
        {
            BasicBlock* runner = predecessor;
            while (runner != dominator)
            {
                df[runner].emplace_back(block);
                runner = idom.at(runner);
            }
        }
    }

    return df;
}

PhiList ToSSA::calculatePhiNodes(const DomFrontier& df)
{
    PhiList result;

    for (Value* local : _function->locals)
    {
        std::deque<BasicBlock*> workList;
        std::set<BasicBlock*> everOnWorkList;
        std::set<BasicBlock*> alreadyInserted;

        // Gather all blocks containing an assignment to this variable
        for (Instruction* inst : local->uses)
        {
            if (dynamic_cast<StoreInst*>(inst))
            {
                everOnWorkList.insert(inst->parent);
                workList.push_back(inst->parent);
            }
        }

        while (!workList.empty())
        {
            BasicBlock* next = workList.front();
            workList.pop_front();

            if (df.find(next) == df.end())
                continue;

            // Loop over the dominance frontier of this block
            for (BasicBlock* v : df.at(next))
            {
                if (alreadyInserted.find(v) != alreadyInserted.end())
                    continue;

                result[v].emplace_back(local);
                alreadyInserted.insert(v);

                if (everOnWorkList.find(v) == everOnWorkList.end())
                {
                    everOnWorkList.insert(v);
                    workList.push_back(v);
                }
            }
        }
    }

    for (Value* param : _function->params)
    {
        std::deque<BasicBlock*> workList;
        std::set<BasicBlock*> everOnWorkList;
        std::set<BasicBlock*> alreadyInserted;

        // Gather all blocks containing an assignment to this variable
        for (Instruction* inst : param->uses)
        {
            if (dynamic_cast<StoreInst*>(inst))
            {
                everOnWorkList.insert(inst->parent);
                workList.push_back(inst->parent);
            }
        }

        // Function arguments have an ssumed assignment in the entry node
        BasicBlock* entry = _function->blocks[0];
        if (everOnWorkList.find(entry) == everOnWorkList.end())
        {
            everOnWorkList.insert(entry);
            workList.push_back(entry);
        }

        while (!workList.empty())
        {
            BasicBlock* next = workList.front();
            workList.pop_front();

            if (df.find(next) == df.end())
                continue;

            // Loop over the dominance frontier of this block
            for (BasicBlock* v : df.at(next))
            {
                if (alreadyInserted.find(v) != alreadyInserted.end())
                    continue;

                result[v].emplace_back(param);
                alreadyInserted.insert(v);

                if (everOnWorkList.find(v) == everOnWorkList.end())
                {
                    everOnWorkList.insert(v);
                    workList.push_back(v);
                }
            }
        }
    }

    return result;
}

Value* ToSSA::generateName(Value* variable)
{
    int i = _counter[variable]++;
    std::stringstream ss;
    ss << variable->name << "." << i;

    Value* newName = _function->makeTemp(ss.str());
    _phiStack[variable].push(newName);

    return newName;
}

void ToSSA::rename(BasicBlock* block, PhiList& phis)
{
    if (_visited.find(block) == _visited.end())
    {
        _visited.insert(block);
    }
    else
    {
        return;
    }

    // Keep track of the names we've created so that we can easily undo it
    std::vector<Value*> toPop;

    // Rename the LHS of phi nodes
    if (phis.find(block) != phis.end())
    {
        for (PhiDescription& phiDesc : phis.at(block))
        {
            phiDesc.dest = generateName(phiDesc.original);
            toPop.push_back(phiDesc.original);
        }
    }

    // Rewrite load and store instructions with new names
    for (Instruction* inst = block->first; inst != nullptr;)
    {
        if (LoadInst* load = dynamic_cast<LoadInst*>(inst))
        {
            if (_phiStack[load->src].empty())
            {
                // Don't rewrite loads from global variables
                if (dynamic_cast<GlobalValue*>(load->src))
                {
                    inst = inst->next;
                    continue;
                }

                // A load without a previous store should only be possible for
                // a function parameter or a global
                assert(dynamic_cast<Argument*>(load->src));

                // For any node dominated by this one, don't re-load, just
                // use this value
                _phiStack[load->src].push(load->dest);
                toPop.push_back(load->src);
            }
            else
            {
                Value* newName = _phiStack[load->src].top();
                Value* oldName = load->dest;

                inst = inst->next;
                load->removeFromParent();

                _function->replaceReferences(oldName, newName);

                continue;
            }
        }
        else if (StoreInst* store = dynamic_cast<StoreInst*>(inst))
        {
            // Don't rewrite stores to global variables
            if (dynamic_cast<GlobalValue*>(store->dest))
            {
                inst = inst->next;
                continue;
            }

            _phiStack[store->dest].push(store->src);
            toPop.push_back(store->dest);

            inst = inst->next;
            store->removeFromParent();

            continue;
        }

        inst = inst->next;
    }

    // Fix-up phi nodes of successors
    for (BasicBlock* next : block->successors())
    {
        if (phis.find(next) != phis.end())
        {
            for (PhiDescription& phiDesc : phis.at(next))
            {
                if (!_phiStack[phiDesc.original].empty())
                {
                    phiDesc.sources.emplace_back(block, _phiStack[phiDesc.original].top());
                }
                else
                {
                    // It's possible that some predecessor doesn't define the variable at all.
                    // This is probably an indication that the variable isn't live, and we
                    // don't need the phi node, but let's postpone that analysis
                    phiDesc.sources.emplace_back(block, nullptr);
                }
            }
        }
    }

    // Recurse
    for (BasicBlock* next : block->successors())
    {
        rename(next, phis);
    }

    // Remove names from the stack
    for (Value* value : toPop)
    {
        assert(!_phiStack[value].empty());
        _phiStack[value].pop();
    }
}

void ToSSA::insertPhis(PhiList& phis)
{
    for (auto& item : phis)
    {
        BasicBlock* block = item.first;

        for (PhiDescription& phiDesc : item.second)
        {
            assert(phiDesc.dest);

            PhiInst* phi = new PhiInst(phiDesc.dest);
            for (auto& source : phiDesc.sources)
            {
                if (!source.second)
                {
                    // For some path leading to this block, there is no definition
                    // to merge in this phi node.

                    // Case 1: Function argument -- in this case, there is an
                    // implicit store at the beginning of the function. Add an
                    // explicit load in the predecessor
                    if (dynamic_cast<Argument*>(phiDesc.original))
                    {
                        Value* tmp = generateName(phiDesc.original);
                        LoadInst* inst = new LoadInst(tmp, phiDesc.original);
                        inst->insertBefore(source.first->last);
                        source.second = tmp;
                    }

                    // Case 2: Local variable -- in this case, the value really is
                    // undefined, so the result variable better be dead, or we'll
                    // have undefined behavior
                    else
                    {
                        delete phi;
                        phi = nullptr;
                        break;
                    }
                }

                phi->addSource(source.first, source.second);
            }

            if (phi)
                block->prepend(phi);
        }
    }
}

void ToSSA::killDeadPhis()
{
    for (BasicBlock* block : _function->blocks)
    {
        Instruction* inst = block->first;
        while (inst != nullptr)
        {
            if (PhiInst* phi = dynamic_cast<PhiInst*>(inst))
            {
                inst = inst->next;

                if (phi->dest->uses.empty())
                {
                    Value* deadValue = phi->dest;

                    // If the result is dead, then we don't need the phi
                    phi->removeFromParent();

                    // And we don't need the value
                    assert(!deadValue->definition);
                    _function->temps.erase(std::remove(_function->temps.begin(), _function->temps.end(), deadValue), _function->temps.end());
                }
            }
            else
            {
                break;
            }
        }
    }
}