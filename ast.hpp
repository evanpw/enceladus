#ifndef AST_HPP
#define AST_HPP

#include <iostream>
#include <list>
#include <sstream>
#include "string_table.hpp"

class AstVisitor;
struct YYLTYPE;

/* Abstract base nodes */
class AstNode
{
public:
	AstNode();
	virtual ~AstNode();
	virtual const char* str() const  = 0;
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) = 0;
	
	std::stringstream& code() { return code_; }
	YYLTYPE* location() { return location_; }
	
protected:
	std::stringstream code_;
	YYLTYPE* location_;
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
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
private:
	ProgramNode() {}
	std::list<AstNode*> children_;
};

class LabelNode : public AstNode
{
public:
	static LabelNode* create(const char* name);
	virtual const char* str() const { return "Label"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
	const char* name() { return name_; }
	
private:
	LabelNode() {}
	const char* name_;
};

/* Expression nodes */
class NotNode : public ExpressionNode
{
public:
	static NotNode* create(ExpressionNode* expression);
	virtual const char* str() const { return "Not"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
private:
	NotNode() {}
	ExpressionNode* expression_;
};

class BinaryOperatorNode : public ExpressionNode
{
public:
	virtual void show(std::ostream& out, int depth) const;
	
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
	virtual void accept(AstVisitor* visitor);
	
private:
	GreaterNode() {}
};

class EqualNode : public BinaryOperatorNode
{
public:
	static EqualNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Equal"; }
	virtual void accept(AstVisitor* visitor);
	
private:
	EqualNode() {}
};

class PlusNode : public BinaryOperatorNode
{
public:
	static PlusNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Plus"; }
	virtual void accept(AstVisitor* visitor);
	
private:
	PlusNode() {}
};

class MinusNode : public BinaryOperatorNode
{
public:
	static MinusNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Minus"; }
	virtual void accept(AstVisitor* visitor);
	
private:
	MinusNode() {}
};

class TimesNode : public BinaryOperatorNode
{
public:
	static TimesNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Times"; }
	virtual void accept(AstVisitor* visitor);
	
private:
	TimesNode() {}
};

class DivideNode : public BinaryOperatorNode
{
public:
	static DivideNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Divide"; }
	virtual void accept(AstVisitor* visitor);
	
private:
	DivideNode() {}
};

class VariableNode : public ExpressionNode
{
public:
	static VariableNode* create(const char* name);
	virtual const char* str() const { return "Variable"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
	const char* name() { return name_; }
	
private:
	VariableNode() {}
	const char* name_;
};

class IntNode : public ExpressionNode
{
public:
	static IntNode* create(int value);
	virtual const char* str() const { return "Int"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
private:
	IntNode() {}
	int value_;
};

/* Statement nodes */
class IfNode : public StatementNode
{
public:
	static IfNode* create(ExpressionNode* condition, StatementNode* body);
	virtual const char* str() const { return "If"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
private:
	IfNode() {}
	ExpressionNode* condition_;
	StatementNode* body_;
};

class GotoNode : public StatementNode
{
public:
	static GotoNode* create(const char* target);
	virtual const char* str() const { return "Goto"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
	const char* target() const { return target_; }
	
private:
	GotoNode() {}
	const char* target_;
};

class PrintNode : public StatementNode
{
public:
	static PrintNode* create(ExpressionNode* expression);
	virtual const char* str() const { return "Print"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
private:
	PrintNode() {}
	ExpressionNode* expression_;
};

class ReadNode : public StatementNode
{
public:
	static ReadNode* create(VariableNode* target);
	virtual const char* str() const { return "Read"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
private:
	ReadNode() {}
	VariableNode* target_;
};

class AssignNode : public StatementNode
{
public:
	static AssignNode* create(VariableNode* target, ExpressionNode* value);
	virtual const char* str() const { return "Assign"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor);
	
private:
	AssignNode() {}
	VariableNode* target_;
	ExpressionNode* value_;
};

/* Visitor class */

class AstVisitor
{
public:
	virtual void visit(ProgramNode* node) {}
	virtual void visit(LabelNode* node) {}
	virtual void visit(NotNode* node) {}
	virtual void visit(GreaterNode* node) {}
	virtual void visit(EqualNode* node) {}
	virtual void visit(PlusNode* node) {}
	virtual void visit(MinusNode* node) {}
	virtual void visit(TimesNode* node) {}
	virtual void visit(DivideNode* node) {}
	virtual void visit(VariableNode* node) {}
	virtual void visit(IntNode* node) {}
	virtual void visit(IfNode* node) {}
	virtual void visit(GotoNode* node) {}
	virtual void visit(PrintNode* node) {}
	virtual void visit(ReadNode* node) {}
	virtual void visit(AssignNode* node) {}
};

#endif
