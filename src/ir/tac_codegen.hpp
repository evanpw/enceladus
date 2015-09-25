#ifndef TAC_CODE_GEN_HPP
#define TAC_CODE_GEN_HPP

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include "ir/context.hpp"
#include "ir/tac_instruction.hpp"
#include "ir/value.hpp"

#include <deque>

#define UNSUPPORTED(T) virtual void visit(T* node) { assert(false); }

class TACCodeGen;

class TACConditionalCodeGen : public AstVisitor
{
public:
    TACConditionalCodeGen(TACCodeGen* mainCodeGen);

    UNSUPPORTED(AssertNode);
    UNSUPPORTED(AssignNode);
    UNSUPPORTED(BlockNode);
    UNSUPPORTED(BreakNode);
    UNSUPPORTED(DataDeclaration);
    UNSUPPORTED(ForeachNode);
    UNSUPPORTED(ForeverNode);
    UNSUPPORTED(ForNode);
    UNSUPPORTED(FunctionDefNode);
    UNSUPPORTED(IfElseNode);
    UNSUPPORTED(IfNode);
    UNSUPPORTED(ImplNode);
    UNSUPPORTED(IntNode);
    UNSUPPORTED(LetNode);
    UNSUPPORTED(MatchArm);
    UNSUPPORTED(MatchNode);
    UNSUPPORTED(MemberDefNode);
    UNSUPPORTED(MethodDeclNode);
    UNSUPPORTED(MethodDefNode);
    UNSUPPORTED(PassNode);
    UNSUPPORTED(ProgramNode);
    UNSUPPORTED(ReturnNode);
    UNSUPPORTED(StringLiteralNode);
    UNSUPPORTED(StructDefNode);
    UNSUPPORTED(TypeAliasNode);
    UNSUPPORTED(VariableDefNode);
    UNSUPPORTED(WhileNode);

    virtual void visit(BoolNode* node) { wrapper(node); }
    virtual void visit(MemberAccessNode* node) { wrapper(node); }
    virtual void visit(NullaryNode* node) { wrapper(node); }
    virtual void visit(VariableNode* node) { wrapper(node); }
    virtual void visit(MethodCallNode* node) { wrapper(node); }

    virtual void visit(FunctionCallNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(LogicalNode* node);

    void visitCondition(AstNode& node, BasicBlock* trueBranch, BasicBlock* falseBranch)
    {
        BasicBlock* saveTrueBranch = _trueBranch;
        BasicBlock* saveFalseBranch = _falseBranch;

        _trueBranch = trueBranch;
        _falseBranch = falseBranch;

        node.accept(this);

        _trueBranch = saveTrueBranch;
        _falseBranch = saveFalseBranch;
    }

    void wrapper(AstNode* node)
    {
        Value* value = visitAndGet(node);
        emit(new JumpIfInst(value, _trueBranch, _falseBranch));
    }

private:
    void setBlock(BasicBlock* block);

    void emit(Instruction* inst);
    Value* visitAndGet(AstNode* node);
    BasicBlock* createBlock();

    BasicBlock* _trueBranch;
    BasicBlock* _falseBranch;

    TACCodeGen* _mainCodeGen;
    TACContext* _context;
};

class TACCodeGen : public AstVisitor
{
public:
    TACCodeGen(TACContext* context);

    void codeGen(AstContext* astContext);

    virtual void visit(AssertNode* node);
    virtual void visit(AssignNode* node);
    virtual void visit(BlockNode* node);
    virtual void visit(BoolNode* node);
    virtual void visit(BreakNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(DataDeclaration* node);
    virtual void visit(ForeachNode* node);
    virtual void visit(ForeverNode* node);
    virtual void visit(ForNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(IfNode* node);
    virtual void visit(ImplNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(LetNode* node);
    virtual void visit(LogicalNode* node);
    virtual void visit(MatchArm* node);
    virtual void visit(MatchNode* node);
    virtual void visit(MemberAccessNode* node);
    virtual void visit(MethodCallNode* node);
    virtual void visit(NullaryNode* node);
    virtual void visit(ProgramNode* node);
    virtual void visit(ReturnNode* node);
    virtual void visit(StringLiteralNode* node);
    virtual void visit(StructDefNode* node);
    virtual void visit(VariableDefNode* node);
    virtual void visit(VariableNode* node);
    virtual void visit(WhileNode* node);

    // No code to generate (or handled separately)
    virtual void visit(FunctionDefNode* node) {}
    virtual void visit(MemberDefNode* node) {}
    virtual void visit(MethodDefNode*) {}
    virtual void visit(PassNode* node) {}
    virtual void visit(TypeAliasNode* node) {}

private:
    // We cache the Value corresponding to each symbol so that the value
    // uniquely identifies a location
    std::unordered_map<const Symbol*, Value*> _names;
    Value* getValue(const Symbol* symbol);

    // The exit label of the current loop (used by break statements)
    BasicBlock* _currentLoopExit;

    // Visit the given node and return its value
    Value* visitAndGet(AstNode* node)
    {
        node->accept(this);
        return node->value;
    }

    // We accumulate these lists while walking through the top level, and then
    // generate code for each of them after the main function is finished
    std::deque<FunctionDefNode*> _functions;
    std::vector<ConstructorSymbol*> _constructors;
    void createConstructor(ValueConstructor* constructor);

    // Current assignment of type variables to types
    std::unordered_map<TypeVariable*, Type*> _typeContext;

    TACContext* _context;
    Function* _currentFunction;
    Value* _currentSwitchExpr = nullptr;

    TACConditionalCodeGen _conditionalCodeGen;
    friend class TACConditionalCodeGen;

    int64_t _nextSeqNumber = 0;
    Value* createTemp(ValueType type)
    {
        return _currentFunction->createTemp(type);
    }

    BasicBlock* createBlock()
    {
        return _currentFunction->createBlock();
    }

    void setBlock(BasicBlock* block) { _currentBlock = block; }
    BasicBlock* _currentBlock = nullptr;

    void emit(Instruction* inst);
};

#endif
