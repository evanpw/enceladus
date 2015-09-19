#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast/ast.hpp"
#include "semantic/symbol_table.hpp"

#include <map>
#include <vector>

class SemanticError : public std::exception
{
public:
    SemanticError(const std::string& description)
    : _description(description)
    {}

    virtual ~SemanticError() throw() {}

    const std::string& description() const { return _description; }
    virtual const char* what() const throw() { return _description.c_str(); }


private:
    std::string _description;
};

class SemanticAnalyzer : public AstVisitor
{
public:
    SemanticAnalyzer(AstContext* context);
    bool analyze();

    // Declarations
    virtual void visit(DataDeclaration* node);
    virtual void visit(ForeignDeclNode* node);
    virtual void visit(FunctionDefNode* node);
    virtual void visit(VariableDefNode* node);
    virtual void visit(StructDefNode* node);
    virtual void visit(TypeAliasNode* node);

    // Internal nodes
    virtual void visit(AssignNode* node);
    virtual void visit(BlockNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(ConstructorSpec* node);
    virtual void visit(ForeachNode* node);
    virtual void visit(ForeverNode* node);
    virtual void visit(ForNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(IfNode* node);
    virtual void visit(LogicalNode* node);
    virtual void visit(MatchArm* node);
    virtual void visit(LetNode* node);
    virtual void visit(ProgramNode* node);
    virtual void visit(MatchNode* node);
    virtual void visit(WhileNode* node);

    // Leaf nodes
    virtual void visit(BoolNode* node);
    virtual void visit(BreakNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(MemberAccessNode* node);
    virtual void visit(MemberDefNode* node);
    virtual void visit(NullaryNode* node);
    virtual void visit(ReturnNode* node);
    virtual void visit(StringLiteralNode* node);
    virtual void visit(VariableNode* node);

    // Work in progress
    virtual void visit(FunctionDeclNode* node);
    virtual void visit(TraitDefNode* node);
    virtual void visit(TraitImplNode* node);
    virtual void visit(ImplNode* node);

private:
    //// Type Inference ////////////////////////////////////////////////////////
    Type* newVariable();

    static void inferenceError(AstNode* node, const std::string& msg);

    static bool occurs(TypeVariable* variable, Type* value);
    void unify(Type* lhs, Type* rhs, AstNode* node);
    void bindVariable(Type* variable, Type* value, AstNode* node);

    Type* instantiate(Type* type);
    Type* instantiate(Type* type, std::map<TypeVariable*, Type*>& replacements);

    //// General semantic analysis /////////////////////////////////////////////
    template<typename... Args>
    void semanticError(const YYLTYPE& location, const std::string& str, Args... args);

    FunctionSymbol* createBuiltin(const std::string& name);
    FunctionSymbol* createExternal(const std::string& name);
    void injectSymbols();

    TypeConstructor* getTypeConstructor(const TypeName* typeName);
    TypeConstructor* getTypeConstructor(const YYLTYPE& location, const std::string& name);
    void resolveBaseType(TypeName* typeName, const std::unordered_map<std::string, Type*>& variables);
    void resolveTypeName(TypeName* typeName, const std::unordered_map<std::string, Type*>& variables = {});

    Symbol* resolveSymbol(const std::string& name);
    Symbol* resolveTypeSymbol(const std::string& name);

    ProgramNode* _root;
    AstContext* _context;
    TypeTable* _typeTable;
    SymbolTable* _symbolTable;
    FunctionDefNode* _enclosingFunction;
    LoopNode* _enclosingLoop;
    TraitDefNode* _enclosingTrait;
    ImplNode* _enclosingImplNode;
};

class TypeInferenceError : public std::exception
{
public:
    TypeInferenceError(const std::string& description)
    : _description(description)
    {}

    virtual ~TypeInferenceError() throw() {}

    const std::string& description() const { return _description; }
    virtual const char* what() const throw() { return _description.c_str(); }


private:
    std::string _description;
};

#endif
