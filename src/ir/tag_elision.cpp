#include "ir/tag_elision.hpp"
#include "ir/context.hpp"
#include "ir/tac_instruction.hpp"

#include <stack>
#include <unordered_map>

TagElision::TagElision(Function* function)
: _function(function), _context(function->context())
{
}

std::vector<Value*> TagElision::getVariables(PhiInst* phi)
{
    std::vector<Value*> result;

    if (!dynamic_cast<Constant*>(phi->dest))
    {
        result.push_back(phi->dest);
    }

    for (auto& item : phi->sources())
    {
        if (!dynamic_cast<Constant*>(item.second))
        {
            result.push_back(item.second);
        }
    }

    return result;
}

static int64_t getUntagCost(Value* value)
{
    int64_t cost = 0;

    bool needTagged = false;
    for (Instruction* use : value->uses)
    {
        if (UntagInst* untag = dynamic_cast<UntagInst*>(use))
        {
            assert(untag->src == value);
            // We can get rid of this instruction if we untag this variable
            cost -= 1;
        }
        else if (ConditionalJumpInst* conditional = dynamic_cast<ConditionalJumpInst*>(use))
        {
            // If the other argument is an immediate, then we can adjust the instruction
            // at no cost. Otherwise, we'll have to tag first.
            if (!dynamic_cast<ConstantInt*>(conditional->rhs) &&
                !dynamic_cast<ConstantInt*>(conditional->lhs))
            {
                cost += 1;
            }
        }
        else if (dynamic_cast<PhiInst*>(use))
        {
            // Cost depends on whether the other variables are untagged.
            // Defer this calculation.
        }
        else
        {
            // Any other use case will require that we re-tag first
            needTagged = true;
        }
    }

    if (needTagged)
    {
        cost += 1;
    }

    assert(value->definition);

    if (TagInst* tag = dynamic_cast<TagInst*>(value->definition))
    {
        assert(tag->dest == value);
        // We can get rid of this instruction if we untag this variable
        cost -= 1;
    }
    else if (dynamic_cast<PhiInst*>(value->definition))
    {
        // Cost depends on whether the other variables are untagged.
        // Defer this calculation.
    }
    else
    {
        // Otherwise, we'll have to untag the variable after creating it
        cost += 1;
    }

    return cost;
}

std::ostream& operator<<(std::ostream& out, const std::unordered_set<Value*>& variables)
{
    out << "{";
    for (Value* value : variables)
    {
        out << " " << value->str();
    }
    out << " }";

    return out;
}

void TagElision::run()
{
    // Get all variables which are the destination of a tag instruction, or the
    // source of an untag instruction
    GatherVariables gatherVariables(_taggedVariables);
    for (BasicBlock* block : _function->blocks)
    {
        for (Instruction* inst = block->first; inst != nullptr; inst = inst->next)
        {
            inst->accept(&gatherVariables);
        }
    }

    // Find the connected components containing each of these tagged variables,
    // where two variables are connected by an edge whenever they are related
    // by a phi instruction

    // First, compute the full phi graph
    std::unordered_map<Value*, std::unordered_set<Value*>> graph;
    for (BasicBlock* block : _function->blocks)
    {
        for (Instruction* inst = block->first; inst != nullptr; inst = inst->next)
        {
            if (PhiInst* phi = dynamic_cast<PhiInst*>(inst))
            {
                std::vector<Value*> values = getVariables(phi);

                for (size_t i = 0; i < values.size() - 1; ++i)
                {
                    for (size_t j = i + 1; j < values.size(); ++j)
                    {
                        graph[values[i]].insert(values[j]);
                        graph[values[j]].insert(values[i]);
                    }
                }
            }
            else
            {
                // Phi instructions must all be at the beginning of a basic block
                break;
            }
        }
    }

    // Find the connected components
    std::vector<std::unordered_set<Value*>> components;
    while (!_taggedVariables.empty())
    {
        // Pop a value from the set of roots
        auto i = _taggedVariables.begin();
        Value* root = *i;
        _taggedVariables.erase(i);

        std::stack<Value*> open;
        std::unordered_set<Value*> component;

        open.push(root);

        while (!open.empty())
        {
            Value* current = open.top();
            open.pop();

            // If we've already seen this variable skip it
            if (component.find(current) != component.end())
                continue;

            component.insert(current);

            // If this variable is a root, remove it from the list
            auto j = _taggedVariables.find(current);
            if (j != _taggedVariables.end())
            {
                _taggedVariables.erase(j);
            }

            // Recurse to all neighbors
            for (Value* neighbor : graph[current])
            {
                open.push(neighbor);
            }
        }

        components.push_back(component);
    }


    //std::cerr << "Tagged variables:" << std::endl;

    for (auto& component : components)
    {
        // std::cerr << "\t{";
        // for (Value* value : component)
        // {
        //     std::cerr << " " << value->str() << "(" << getUntagCost(value) << ")";
        // }
        // std::cerr << " }" << std::endl;

        // If the component is too big, this brute-force approach will be
        // unbearably slow
        assert(component.size() < 20);

        // Iterate over the power set of this set of variables
        size_t bestMask = 0;
        int64_t bestCost = 0;
        size_t bestSize = 0;
        for (size_t mask = 1; mask < size_t(1 << component.size()); ++mask)
        {
            // Split the component into those variables we will untag, and those
            // we won't
            std::unordered_set<Value*> untagged;
            std::unordered_set<Value*> tagged;

            size_t bit = 1;
            for (Value* value : component)
            {
                if (mask & bit)
                {
                    untagged.insert(value);
                }
                else
                {
                    tagged.insert(value);
                }

                bit <<= 1;
            }

            // Compute the total cost of this splitting as the sum of the costs
            // of each untagged variable, plus the number of edges between the
            // untagged and tagged variables (i.e., the number of phi nodes
            // relating variables in those two groups).
            int64_t totalCost = 0;
            for (Value* value : untagged)
            {
                totalCost += getUntagCost(value);

                for (Value* neighbor : graph[value])
                {
                    if (tagged.find(neighbor) != tagged.end())
                    {
                        totalCost += 1;
                    }
                }
            }

            // Among splittings with the same cost, prefer the one with the
            // smallest number of untagged variables
            if (totalCost < bestCost || (totalCost == bestCost && untagged.size() < bestSize))
            {
                bestMask = mask;
                bestCost = totalCost;
                bestSize = untagged.size();
            }
        }

        // Create a name for the untagged version of each variable. If the
        // variable is created using an explicit tag instruction, use the RHS.
        for (Value* value : component)
        {
            Value* untagged = getAlreadyUntagged(value);
            if (untagged)
            {
                _taggedToUntagged[value] = untagged;
            }
            else
            {
                std::stringstream name;

                if (!value->name.empty())
                {
                    name << value->name << ".u";
                }
                else
                {
                    name << value->seqNumber << ".u";
                }

                _taggedToUntagged[value] = _function->createTemp(ValueType::Integer, name.str());
            }
        }

        size_t bit = 1;
        std::unordered_set<Value*> bestUntagged;
        for (Value* value : component)
        {
            if (bestMask & bit)
            {
                bestUntagged.insert(value);
                untagValue(value, true);
            }
            else
            {
                untagValue(value, false);
            }

            RewriteUses rewriteUses(_function, value, _taggedToUntagged);
            rewriteUses.run();

            bit <<= 1;
        }

        //std::cerr << "\tBest: " << bestUntagged << " -> " << bestCost << std::endl;
    }
    //std::cerr << std::endl;

    // std::cerr << "Locals:" << std::endl;
    // for (Value* local : _function->locals)
    // {
    //     std::cerr << "\t" << local->str() << " : " << valueTypeString(local->type) << std::endl;
    // }

    // std::cerr << "Temps:" << std::endl;
    // for (Value* temp : _function->temps)
    // {
    //     std::cerr << "\t" << temp->str() << " : " << valueTypeString(temp->type) << std::endl;
    // }
}

// Get the untagged version of the given value if it is created by an explicit
// tag instruction. Otherwise, returns null
Value* TagElision::getAlreadyUntagged(Value* value)
{
    Instruction* definition = value->definition;
    assert(definition);

    if (TagInst* tagInst = dynamic_cast<TagInst*>(definition))
    {
        return tagInst->src;
    }
    else
    {
        return nullptr;
    }
}

// Insert untag instructions or rewrite phi nodes to produce an untagged version
// of the given variable
void TagElision::untagValue(Value* tagged, bool rewritePhi)
{
    Value* untagged = _taggedToUntagged.at(tagged);

    Instruction* definition = tagged->definition;
    assert(definition);

    if (dynamic_cast<TagInst*>(definition))
    {
        return;
    }

    if (rewritePhi)
    {
        if (PhiInst* phiInst = dynamic_cast<PhiInst*>(definition))
        {
            PhiInst* newPhi = new PhiInst(untagged);
            for (auto& source : phiInst->sources())
            {
                BasicBlock* block = source.first;
                Value* taggedSource = source.second;

                Value* untaggedSource;
                if (ConstantInt* imm = dynamic_cast<ConstantInt*>(taggedSource))
                {
                    untaggedSource = _context->getConstantInt((imm->value) >> 1);
                }
                else
                {
                    untaggedSource = _taggedToUntagged.at(taggedSource);
                }

                newPhi->addSource(block, untaggedSource);
            }

            phiInst->replaceWith(newPhi);

            // Now insert a tag instruction to recreate the tagged version
            Instruction* inst = newPhi;
            while (dynamic_cast<PhiInst*>(inst->next))
            {
                inst = inst->next;
            }

            assert(inst);

            TagInst* tagInst = new TagInst(tagged, untagged);
            tagInst->insertAfter(inst);

            return;
        }
    }

    // Skip any phi nodes at the beginning of the block
    Instruction* inst = definition;
    while (dynamic_cast<PhiInst*>(inst->next))
    {
        inst = inst->next;
    }

    // phi is not a terminating instruction
    assert(inst);

    UntagInst* untagInst = new UntagInst(untagged, tagged);
    untagInst->insertAfter(inst);
}

void TagElision::GatherVariables::visit(TagInst* inst)
{
    if (!isConstant(inst->dest))
        _taggedVariables.insert(inst->dest);
}

void TagElision::GatherVariables::visit(UntagInst* inst)
{
    if (!isConstant(inst->src))
        _taggedVariables.insert(inst->src);
}

void TagElision::RewriteUses::run()
{
    // Make a copy, because it might be modified while looping through
    auto uses = _tagged->uses;

    for (Instruction* inst : uses)
    {
        inst->accept(this);
    }
}

void TagElision::RewriteUses::visit(UntagInst* inst)
{
    assert(inst->src == _tagged);

    if (inst->dest != _untagged)
    {
        Value* dest = inst->dest;
        inst->removeFromParent();

        _function->replaceReferences(dest, _untagged);
    }
}

void TagElision::RewriteUses::visit(ConditionalJumpInst* inst)
{
    Value* lhs = inst->lhs;
    Value* rhs = inst->rhs;

    if (rhs == _tagged)
    {
        std::swap(lhs, rhs);
    }

    assert(lhs == _tagged);
    if (ConstantInt* constInt = dynamic_cast<ConstantInt*>(rhs))
    {
        int64_t untaggedValue = constInt->value >> 1;
        Value* newRhs = _function->context()->getConstantInt(untaggedValue);

        inst->replaceReferences(_tagged, _untagged);
        inst->replaceReferences(rhs, newRhs);
    }
    else
    {
        auto i = _mapping.find(rhs);
        if (i != _mapping.end())
        {
            inst->replaceReferences(_tagged, _untagged);
            inst->replaceReferences(rhs, i->second);
        }
    }
}
