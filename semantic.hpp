#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast.hpp"

class SemanticAnalyzer
{
public:
	SemanticAnalyzer(ProgramNode* root);
	bool analyze();
	
private:
	ProgramNode* root_;
};

class SemanticPass1 : public AstVisitor
{
public:
	SemanticPass1();
	virtual void visit(LabelNode* node);
	virtual void visit(VariableNode* node);
	
	bool success() const { return success_; }
	
private:
	bool success_;
};

class SemanticPass2 : public AstVisitor
{
public:
	SemanticPass2();
	virtual void visit(GotoNode* node);
	
	bool success() const { return success_; }
	
private:
	bool success_;
};

#endif
