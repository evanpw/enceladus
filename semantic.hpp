#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include <map>
#include <vector>
#include "ast.hpp"

class SemanticAnalyzer
{
public:
	SemanticAnalyzer(ProgramNode* root);
	bool analyze();

private:
	ProgramNode* root_;
};

class SemanticBase : public AstVisitor
{
public:
	SemanticBase() : success_(true) {}

	void semanticError(AstNode* node, const std::string& msg);

	bool success() const { return success_; }

private:
	bool success_;
};

// Semantic analysis pass 1 - handle declarations and build the symbol tables
class SemanticPass1 : public SemanticBase
{
public:
	SemanticPass1() : _enclosingFunction(nullptr) {}

	virtual void visit(ProgramNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(ForeignDeclNode* node);
	virtual void visit(DataDeclaration* node);

private:
	FunctionDefNode* _enclosingFunction;
};

// Semantic analysis pass 2 - gotos / function calls
class SemanticPass2 : public SemanticBase
{
public:
	SemanticPass2() : _enclosingFunction(nullptr) {}

	virtual void visit(LetNode* node);
	virtual void visit(MatchNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(NullaryNode* node);
	virtual void visit(AssignNode* node);

private:
	FunctionDefNode* _enclosingFunction;
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

// Pass 3
class TypeChecker : public SemanticBase
{
public:
    TypeChecker(bool verbose = false)
    : _enclosingFunction(nullptr), _verbose(verbose)
    {}

    // Internal nodes
    virtual void visit(AssignNode* node);
    virtual void visit(BinaryOperatorNode* node);
    virtual void visit(BlockNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(FunctionDefNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(IfNode* node);
    virtual void visit(LetNode* node);
    virtual void visit(LogicalNode* node);
    virtual void visit(MatchNode* node);
    virtual void visit(NullNode* node);
    virtual void visit(ProgramNode* node);
    virtual void visit(WhileNode* node);

    // Leaf nodes
    virtual void visit(BoolNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(NilNode* node);
    virtual void visit(NullaryNode* node);
    virtual void visit(ReturnNode* node);

    // Declaration nodes with void type
    virtual void visit(DataDeclaration* node) { node->setType(typeTable_->getBaseType("Unit")); }
    virtual void visit(ForeignDeclNode* node) { node->setType(typeTable_->getBaseType("Unit")); }

private:
    bool occurs(TypeVariable* variable, const std::shared_ptr<Type>& value);
    void unify(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs);
    void bindVariable(const std::shared_ptr<Type>& variable, const std::shared_ptr<Type>& value);

    std::unique_ptr<TypeScheme> generalize(const std::shared_ptr<Type>& type);
    std::shared_ptr<Type> instantiate(const std::shared_ptr<Type>& type, const std::map<TypeVariable*, std::shared_ptr<Type>>& replacements);
    std::shared_ptr<Type> instantiate(TypeScheme* scheme);

    std::set<TypeVariable*> getFreeVars(Symbol* symbol);

private:
    FunctionDefNode* _enclosingFunction;
    bool _verbose;
};

#endif
