#ifndef TAC_CODE_GEN_HPP
#define TAC_CODE_GEN_HPP

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include "ir/context.hpp"
#include "ir/tac_instruction.hpp"
#include "ir/value.hpp"
#include "semantic/types.hpp"

#include <deque>
#include <stdexcept>

class TACCodeGen;

class CodegenError : public std::exception
{
public:
    CodegenError(const std::string& description)
    : _description(description)
    {}

    virtual ~CodegenError() throw() {}

    const std::string& description() const { return _description; }
    virtual const char* what() const throw() { return _description.c_str(); }

private:
    std::string _description;
};

struct MonomorphizationError : public std::exception
{
    virtual ~MonomorphizationError() throw() {}
};

class TACConditionalCodeGen : public SparseAstVisitor
{
public:
    TACConditionalCodeGen(TACCodeGen* mainCodeGen);

    virtual void visit(BoolNode* node) { wrapper(node); }
    virtual void visit(IndexNode* node) { wrapper(node); }
    virtual void visit(MemberAccessNode* node) { wrapper(node); }
    virtual void visit(MethodCallNode* node) { wrapper(node); }
    virtual void visit(NullaryNode* node) { wrapper(node); }

    virtual void visit(ComparisonNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(LetNode* node);
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

class TACAssignmentCodeGen : public SparseAstVisitor
{
public:
    TACAssignmentCodeGen(TACCodeGen* mainCG, Value* value)
    : _mainCG(mainCG), _value(value)
    {}

    virtual void visit(NullaryNode* node);
    virtual void visit(MemberAccessNode* node);
    virtual void visit(IndexNode* node);

private:
    TACCodeGen* _mainCG;
    Value* _value;
};

class TACCodeGen : public AstVisitor
{
public:
    TACCodeGen(TACContext* context);

    void codeGen(AstContext* astContext);

    virtual void visit(AssertNode* node);
    virtual void visit(AssignNode* node);
    virtual void visit(BinopNode* node);
    virtual void visit(BlockNode* node);
    virtual void visit(BoolNode* node);
    virtual void visit(BreakNode* node);
    virtual void visit(CastNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(ContinueNode* node);
    virtual void visit(EnumDeclaration* node);
    virtual void visit(ForeverNode* node);
    virtual void visit(ForNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(ImplNode* node);
    virtual void visit(IndexNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(LambdaNode* node);
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
    virtual void visit(WhileNode* node);

    // No code to generate (or handled separately)
    virtual void visit(FunctionDefNode* node) {}
    virtual void visit(StructVarNode* node) {}
    virtual void visit(MethodDefNode*) {}
    virtual void visit(PassNode* node) {}
    virtual void visit(TypeAliasNode* node) {}

private:
    void makeArray(Type* functionType, bool zero = false);
    void letHelper(LetNode* node, Value* rhs);

private:
    friend class TACAssignmentCodeGen;

    // We cache the Value corresponding to each symbol so that the value
    // uniquely identifies a location
    std::unordered_map<const Symbol*, Value*> _globalNames;
    std::unordered_map<const Symbol*, Value*> _localNames;
    std::unordered_map<const Symbol*, Value*> _externFunctions;
    std::unordered_map<const Symbol*, std::vector<std::pair<TypeAssignment, Function*>>> _functionNames;

    Value* load(const Symbol* symbol);
    void store(const Symbol* symbol, Value* src);

    Value* getValue(const Symbol* symbol);
    Value* getFunctionValue(const Symbol* symbol, AstNode* node, const TypeAssignment& typeAssignment = {});
    Value* getTraitMethodValue(Type* objectType, const Symbol* symbol, AstNode* node, const TypeAssignment& typeAssignment = {});

    Type* getConcreteType(Type* type, const TypeAssignment& typeAssignment = {});
    ValueType getValueType(Type* type, const TypeAssignment& typeAssignment = {});

    std::unordered_map<Function*, uint64_t> _constructorLayouts;
    uint64_t getConstructorLayout(const ConstructorSymbol* symbol, AstNode* node, const TypeAssignment& typeAssignment = {});

    // The entry / exit labels of the current loop (used by break & continue)
    BasicBlock* _currentLoopEntry;
    BasicBlock* _currentLoopExit;

    // Visit the given node and return its value
    Value* visitAndGet(AstNode* node)
    {
        node->accept(this);
        return node->value;
    }

    // We accumulate these lists while walking through the top level, and then
    // generate code for each of them after the main function is finished
    std::vector<ConstructorSymbol*> _constructors;
    void createConstructor(const ConstructorSymbol* constructor, const TypeAssignment& typeAssignment);

    void createClosure(Value* dest, Value* fn, const std::vector<Symbol*>& captures = {});

    // Current assignment of type variables to types
    TypeAssignment _typeContext;
    std::deque<std::pair<const Symbol*, TypeAssignment>> _functions;

    AstContext* _astContext;
    TACContext* _context;

    Function* _currentFunction;
    Value* _currentSwitchExpr = nullptr;

    TACConditionalCodeGen _conditionalCodeGen;
    friend class TACConditionalCodeGen;

    Value* createTemp(ValueType type = ValueType::U64)
    {
        return _currentFunction->createTemp(type);
    }

    BasicBlock* createBlock()
    {
        return _currentFunction->createBlock();
    }

    ConstantInt* constant(uint64_t value)
    {
        return _context->createConstantInt(ValueType::U64, value);
    }

    Value* _gcAllocate = nullptr;

    void gcAllocate(Value* dest, Value* size)
    {
        CallInst* callInst = new CallInst(dest, _gcAllocate, {size});
        callInst->regpass = true;
        emit(callInst);
    }

    void gcAllocate(Value* dest, size_t size)
    {
        gcAllocate(dest, constant(size));
    }

    void setBlock(BasicBlock* block) { _currentBlock = block; }
    BasicBlock* _currentBlock = nullptr;

    void emit(Instruction* inst);
};

#endif
