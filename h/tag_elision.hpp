#ifndef TAG_ELISION_HPP
#define TAG_ELISION_HPP

#include "function.hpp"
#include "tac_visitor.hpp"
#include "value.hpp"

#include <unordered_map>
#include <unordered_set>

class TagElision : public TACVisitor
{
public:
    TagElision(Function* function);
    void run();

private:
    Function* _function;
    TACContext* _context;

    // Visits each instruction and gets a list of all variables which are
    // tagged integers
    class GatherVariables : public TACVisitor
    {
    public:
        GatherVariables(std::unordered_set<Value*>& taggedVariables)
        : _taggedVariables(taggedVariables)
        {}

        virtual void visit(TagInst* inst);
        virtual void visit(UntagInst* inst);

        static bool isConstant(Value* value)
        {
            return dynamic_cast<Constant*>(value);
        }

    private:
        std::unordered_set<Value*>& _taggedVariables;
    };

    // Rewrite all amenable uses of the tagged variable to use the untagged
    // variable instead
    class RewriteUses : public TACVisitor
    {
    public:
        RewriteUses(Function* function, Value* tagged, std::unordered_map<Value*, Value*>& mapping)
        : _function(function), _tagged(tagged), _mapping(mapping)
        {
            _untagged = _mapping.at(tagged);
        }

        void run();

        virtual void visit(UntagInst* inst);
        virtual void visit(ConditionalJumpInst* inst);

    private:
        Function* _function;

        Value* _tagged;
        Value* _untagged;

        std::unordered_map<Value*, Value*>& _mapping;
    };

    std::vector<Value*> getVariables(PhiInst* phi);

    Value* getAlreadyUntagged(Value* value);
    void untagValue(Value* tagged, bool rewritePhi);
    std::unordered_map<Value*, Value*> _taggedToUntagged;

    std::unordered_set<Value*> _taggedVariables;
};

#endif