#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

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

private:
	FunctionDefNode* _enclosingFunction;
};

// Semantic analysis pass 2 - gotos / function calls
class SemanticPass2 : public SemanticBase
{
public:
	virtual void visit(LetNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(NullaryNode* node);
	virtual void visit(AssignNode* node);

private:
	FunctionDefNode* _enclosingFunction;
};

// Pass 3
class TypeChecker : public SemanticBase
{
public:
	TypeChecker() : _enclosingFunction(nullptr) {}

	// Internal nodes
	virtual void visit(ProgramNode* node);
	virtual void visit(ComparisonNode* node);
	virtual void visit(BinaryOperatorNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(WhileNode* node);
	virtual void visit(AssignNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(HeadNode* node);
	virtual void visit(TailNode* node);
	virtual void visit(NullNode* node);

	// Leaf nodes
	virtual void visit(NullaryNode* node);
	virtual void visit(IntNode* node);
	virtual void visit(BoolNode* node);
	virtual void visit(NilNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(ReturnNode* node);

private:
	void typeCheck(AstNode* node, const Type* type);
	FunctionDefNode* _enclosingFunction;
};

#endif
