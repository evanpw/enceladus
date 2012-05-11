#ifndef AST_HPP
#define AST_HPP

#include <iostream>
#include <list>
#include "string_table.hpp"

/* Abstract base nodes */
class AstNode
{
public:
	virtual ~AstNode() {};
	virtual const char* str() const { return "AstNode"; }
	virtual void show(std::ostream& out, int indent) const;
};

class StatementNode : public AstNode {};
class ExpressionNode : public AstNode {};

/* Top-level nodes */

class ProgramNode : public AstNode
{
public:
	static ProgramNode* create();
	void prepend(AstNode* child);
	virtual const char* str() const { return "Program"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	ProgramNode() {}
	std::list<AstNode*> children_;
};

class LabelNode : public AstNode
{
public:
	static LabelNode* create(Symbol* symbol);
	virtual const char* str() const { return "Label"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	LabelNode() {}
	Symbol* symbol_;
};

/* Statement nodes */
class IfNode : public StatementNode
{
public:
	static IfNode* create(ExpressionNode* condition, StatementNode* body);
	virtual const char* str() const { return "If"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	IfNode() {}
	ExpressionNode* condition_;
	StatementNode* body_;
};

class GotoNode : public StatementNode
{
public:
	static GotoNode* create(Symbol* target);
	virtual const char* str() const { return "Goto"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	GotoNode() {}
	Symbol* target_;
};

class PrintNode : public StatementNode
{
public:
	static PrintNode* create(ExpressionNode* expression);
	virtual const char* str() const { return "Print"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	PrintNode() {}
	ExpressionNode* expression_;
};

class ReadNode : public StatementNode
{
public:
	static ReadNode* create(Symbol* target);
	virtual const char* str() const { return "Read"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	ReadNode() {}
	Symbol* target_;
};

class AssignNode : public StatementNode
{
public:
	static AssignNode* create(Symbol* target, ExpressionNode* value);
	virtual const char* str() const { return "Assign"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	AssignNode() {}
	Symbol* target_;
	ExpressionNode* value_;
};

/* Expression nodes */
class NotNode : public ExpressionNode
{
public:
	static NotNode* create(ExpressionNode* expression);
	virtual const char* str() const { return "Not"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	NotNode() {}
	ExpressionNode* expression_;
};

class BinaryOperatorNode : public ExpressionNode
{
public:
	virtual void show(std::ostream& out, int indent) const;
	
protected:
	BinaryOperatorNode() {}
	ExpressionNode* lhs_;
	ExpressionNode* rhs_;
};

class GreaterNode : public BinaryOperatorNode
{
public:
	static GreaterNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Greater"; }
	
private:
	GreaterNode() {}
};

class EqualNode : public BinaryOperatorNode
{
public:
	static EqualNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Equal"; }
	
private:
	EqualNode() {}
};

class PlusNode : public BinaryOperatorNode
{
public:
	static PlusNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Plus"; }
	
private:
	PlusNode() {}
};

class MinusNode : public BinaryOperatorNode
{
public:
	static MinusNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Minus"; }
	
private:
	MinusNode() {}
};

class TimesNode : public BinaryOperatorNode
{
public:
	static TimesNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Times"; }
	
private:
	TimesNode() {}
};

class DivideNode : public BinaryOperatorNode
{
public:
	static DivideNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Divide"; }
	
private:
	DivideNode() {}
};

class IdentNode : public ExpressionNode
{
public:
	static IdentNode* create(Symbol* symbol);
	virtual const char* str() const { return "Ident"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	IdentNode() {}
	Symbol* symbol_;
};

class IntNode : public ExpressionNode
{
public:
	static IntNode* create(Symbol* symbol);
	virtual const char* str() const { return "Int"; }
	virtual void show(std::ostream& out, int indent) const;
	
private:
	IntNode() {}
	Symbol* symbol_;
};

#endif
