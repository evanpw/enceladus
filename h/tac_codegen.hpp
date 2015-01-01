#ifndef TAC_CODE_GEN_HPP
#define TAC_CODE_GEN_HPP

#include "address.hpp"
#include "ast.hpp"
#include "ast_visitor.hpp"
#include "tac_instruction.hpp"
#include "tac_program.hpp"

#include <boost/lexical_cast.hpp>

#define UNSUPPORTED(T) virtual void visit(T* node) { assert(false); }

class TACCodeGen;

class TACConditionalCodeGen : public AstVisitor
{
public:
    TACConditionalCodeGen(TACCodeGen* mainCodeGen)
    : _mainCodeGen(mainCodeGen)
    {}

    UNSUPPORTED(AssignNode);
    UNSUPPORTED(BlockNode);
    UNSUPPORTED(BreakNode);
    UNSUPPORTED(DataDeclaration);
    UNSUPPORTED(FunctionDefNode);
    UNSUPPORTED(IfElseNode);
    UNSUPPORTED(IfNode);
    UNSUPPORTED(IntNode);
    UNSUPPORTED(LetNode);
    UNSUPPORTED(MatchNode);
    UNSUPPORTED(ProgramNode);
    UNSUPPORTED(ReturnNode);
    UNSUPPORTED(StructDefNode);
    UNSUPPORTED(WhileNode);
    UNSUPPORTED(MemberDefNode);
    UNSUPPORTED(TypeAliasNode);

    virtual void visit(BoolNode* node) { wrapper(*node); }
    virtual void visit(MemberAccessNode* node) { wrapper(*node); }
    virtual void visit(NullaryNode* node) { wrapper(*node); }
    virtual void visit(VariableNode* node) { wrapper(*node); }

    virtual void visit(FunctionCallNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(LogicalNode* node);

    void visitCondition(AstNode& node, TACLabel* trueBranch, TACLabel* falseBranch)
    {
        TACLabel* saveTrueBranch = _trueBranch;
        TACLabel* saveFalseBranch = _falseBranch;

        _trueBranch = trueBranch;
        _falseBranch = falseBranch;

        node.accept(this);

        _trueBranch = saveTrueBranch;
        _falseBranch = saveFalseBranch;
    }

    void wrapper(AstNode& node)
    {
        std::shared_ptr<Address> address = visitAndGet(node);
        emit(new TACJumpIf(address, _trueBranch));
        emit(new TACJump(_falseBranch));
    }

private:
    void emit(TACInstruction* inst);
    std::shared_ptr<Address> visitAndGet(AstNode& node);

    TACLabel* _trueBranch;
    TACLabel* _falseBranch;

    TACCodeGen* _mainCodeGen;
};

class TACCodeGen : public AstVisitor
{
public:
    TACCodeGen();

    virtual void visit(AssignNode* node);
    virtual void visit(BlockNode* node);
    virtual void visit(BreakNode* node);
    virtual void visit(BoolNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(DataDeclaration* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(FunctionDefNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(IfNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(LetNode* node);
    virtual void visit(LogicalNode* node);
    virtual void visit(MatchNode* node);
    virtual void visit(MemberAccessNode* node);
    virtual void visit(NullaryNode* node);
    virtual void visit(ProgramNode* node);
    virtual void visit(ReturnNode* node);
    virtual void visit(StructDefNode* node);
    virtual void visit(VariableNode* node);
    virtual void visit(WhileNode* node);
    virtual void visit(MemberDefNode* node);
    virtual void visit(TypeAliasNode* node);

    TACProgram& getResult() { return _tacProgram; }

private:
    // We cache the NameAddress corresponding to each symbol so that the address
    // uniquely identifies a location
    std::unordered_map<const Symbol*, std::shared_ptr<Address>> _names;
    std::shared_ptr<Address> getNameAddress(const Symbol* symbol);

    // Add an underscore to every in-language name, so that they don't
    // interfere with compiler-defined names or assembly-language keywords
    std::string mangle(const std::string& name) { return std::string("_") + name; }

    // The exit label of the current loop (used by break statements)
    TACLabel* _currentLoopEnd;

    // Visit the given node and return its address
    std::shared_ptr<Address> visitAndGet(AstNode& node)
    {
        node.accept(this);
        return node.address;
    }

    // We accumulate these lists while walking through the top level, and then
    // generate code for each of them after the main function is finished
    std::vector<FunctionDefNode*> _functions;
    std::vector<DataDeclaration*> _dataDeclarations;
    std::vector<StructDefNode*> _structDeclarations;

    void createConstructor(ValueConstructor* constructor);
    void createDestructor(ValueConstructor* constructor);

    void incref(std::shared_ptr<Address> operand);

    TACProgram _tacProgram;
    TACFunction* _currentFunction;
    std::shared_ptr<Address> _returnValue;
    TACLabel* _functionEnd;

    TACConditionalCodeGen _conditionalCodeGen;
    friend class TACConditionalCodeGen;

    // Number of temporary variables used so far in the current function
    std::shared_ptr<Address> makeTemp() { return std::make_shared<TempAddress>(_currentFunction->numberOfTemps++); }

    TACInstruction* _currentInstruction = nullptr;
    void emit(TACInstruction* inst);
};

#endif