#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include <map>
#include <vector>
#include "ast.hpp"

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

// Semantic analysis pass 1 - handle declarations and build the symbol tables
class SemanticAnalyzer : public AstVisitor
{
public:
    SemanticAnalyzer(ProgramNode* root);
    bool analyze();

    // Declarations
    virtual void visit(DataDeclaration* node);
    virtual void visit(FunctionDefNode* node);
    virtual void visit(ForeignDeclNode* node);
    virtual void visit(LetNode* node);

    // Internal nodes
    virtual void visit(AssignNode* node);
    virtual void visit(BlockNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(IfNode* node);
    virtual void visit(LogicalNode* node);
    virtual void visit(MatchNode* node);
    virtual void visit(ProgramNode* node);
    virtual void visit(WhileNode* node);

    // Leaf nodes
    virtual void visit(BoolNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(StringNode* node);
    virtual void visit(NullaryNode* node);
    virtual void visit(ReturnNode* node);

private:
    void semanticError(AstNode* node, const std::string& msg);
    void injectSymbols(ProgramNode* node);

    ProgramNode* root_;
    FunctionDefNode* _enclosingFunction;

private:
    std::shared_ptr<Type> newVariable();
    std::map<TypeVariable*, std::vector<std::shared_ptr<Type>>> _variables;

    static void inferenceError(AstNode* node, const std::string& msg);

    static bool occurs(TypeVariable* variable, const std::shared_ptr<Type>& value);
    void unify(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs, AstNode* node);
    void bindVariable(const std::shared_ptr<Type>& variable, const std::shared_ptr<Type>& value, AstNode* node);

    static std::unique_ptr<TypeScheme> generalize(const std::shared_ptr<Type>& type, const std::vector<Scope*>& scopes);
    std::shared_ptr<Type> instantiate(const std::shared_ptr<Type>& type, const std::map<TypeVariable*, std::shared_ptr<Type>>& replacements);
    std::shared_ptr<Type> instantiate(TypeScheme* scheme);

    static std::set<TypeVariable*> getFreeVars(Symbol* symbol);
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
