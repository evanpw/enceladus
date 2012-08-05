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

// Pass 3
class TypeChecker : public AstVisitor
{
public:
	TypeChecker();
	
	// Internal nodes
	virtual void visit(ProgramNode* node);
	virtual void visit(NotNode* node);
	virtual void visit(GreaterNode* node);
	virtual void visit(EqualNode* node);
	virtual void visit(PlusNode* node);
	virtual void visit(MinusNode* node);
	virtual void visit(TimesNode* node);
	virtual void visit(DivideNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(PrintNode* node);
	virtual void visit(ReadNode* node);
	virtual void visit(AssignNode* node);
	
	// Leaf nodes
	virtual void visit(LabelNode* node);
	virtual void visit(VariableNode* node);
	virtual void visit(IntNode* node);
	virtual void visit(GotoNode* node);
	
	bool success() const { return success_; }
	
private:
	bool typeCheck(AstNode* node, Type type);
	
	bool success_;
};

#endif
