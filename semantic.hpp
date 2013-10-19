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
	virtual void visit(LabelNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(AssignNode* node);
};

// Semantic analysis pass 2 - gotos / function calls
class SemanticPass2 : public SemanticBase
{
public:
	virtual void visit(GotoNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(VariableNode* node);
};

// Pass 3
class TypeChecker : public SemanticBase
{
public:
	// Internal nodes
	virtual void visit(ProgramNode* node);
	virtual void visit(NotNode* node);
	virtual void visit(ComparisonNode* node);
	virtual void visit(BinaryOperatorNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(PrintNode* node);
	virtual void visit(ReadNode* node);
	virtual void visit(WhileNode* node);
	virtual void visit(AssignNode* node);

	// Leaf nodes
	virtual void visit(VariableNode* node);
	virtual void visit(IntNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(ReturnNode* node);

private:
	void typeCheck(AstNode* node, Type type);
};

#endif
