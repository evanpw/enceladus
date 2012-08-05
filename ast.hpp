#ifndef AST_HPP
#define AST_HPP

#include <iostream>
#include <list>
#include <sstream>
#include "ast_visitor.hpp"
#include "string_table.hpp"
#include "symbol_table.hpp"
#include "types.hpp"

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
	Type type() { return type_; }
	
	void setType(Type type) { type_ = type; }
	
protected:
	std::stringstream code_;
	YYLTYPE* location_;
	Type type_;
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
	
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	const std::list<AstNode*>& children() const { return children_; }
	
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
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	const char* name() { return name_; }
	const Symbol* symbol() { return symbol_; }
	
	void attachSymbol(Symbol* symbol) { symbol_ = symbol; }
	
private:
	LabelNode() {}
	const char* name_;
	Symbol* symbol_;
};

/* Expression nodes */
class NotNode : public ExpressionNode
{
public:
	static NotNode* create(ExpressionNode* expression);
	virtual const char* str() const { return "Not"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	ExpressionNode* child() { return expression_; }
	
private:
	NotNode() {}
	ExpressionNode* expression_;
};

class BinaryOperatorNode : public ExpressionNode
{
public:
	virtual void show(std::ostream& out, int depth) const;
	
	ExpressionNode* lhs() { return lhs_; }
	ExpressionNode* rhs() { return rhs_; }
	
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
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
private:
	GreaterNode() {}
};

class EqualNode : public BinaryOperatorNode
{
public:
	static EqualNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Equal"; }
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
private:
	EqualNode() {}
};

class PlusNode : public BinaryOperatorNode
{
public:
	static PlusNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Plus"; }
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
private:
	PlusNode() {}
};

class MinusNode : public BinaryOperatorNode
{
public:
	static MinusNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Minus"; }
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
private:
	MinusNode() {}
};

class TimesNode : public BinaryOperatorNode
{
public:
	static TimesNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Times"; }
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
private:
	TimesNode() {}
};

class DivideNode : public BinaryOperatorNode
{
public:
	static DivideNode* create(ExpressionNode* lhs, ExpressionNode* rhs);
	virtual const char* str() const { return "Divide"; }
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
private:
	DivideNode() {}
};

class VariableNode : public ExpressionNode
{
public:
	static VariableNode* create(const char* name);
	virtual const char* str() const { return "Variable"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	const char* name() { return name_; }
	const Symbol* symbol() { return symbol_; }
	
	void attachSymbol(Symbol* symbol) { symbol_ = symbol; }
	
private:
	VariableNode() {}
	const char* name_;
	Symbol* symbol_;
};

class IntNode : public ExpressionNode
{
public:
	static IntNode* create(int value);
	virtual const char* str() const { return "Int"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	int value() { return value_; }
	
private:
	IntNode() {}
	int value_;
};

/* Statement nodes */
class BlockNode : public StatementNode
{
public:
	static BlockNode* create();
	void prepend(StatementNode* child);
	virtual const char* str() const { return "Block"; }
	virtual void show(std::ostream& out, int depth) const;
	
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	const std::list<StatementNode*>& children() const { return children_; }
	
private:
	BlockNode() {}
	std::list<StatementNode*> children_;
};

class IfNode : public StatementNode
{
public:
	static IfNode* create(ExpressionNode* condition, StatementNode* body);
	virtual const char* str() const { return "If"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	ExpressionNode* condition() { return condition_; }
	StatementNode* body() { return body_; }
	
private:
	IfNode() {}
	ExpressionNode* condition_;
	StatementNode* body_;
};

class IfElseNode : public StatementNode
{
public:
	static IfElseNode* create(ExpressionNode* condition, StatementNode* body, StatementNode* else_body);
	virtual const char* str() const { return "IfElse"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	ExpressionNode* condition() { return condition_; }
	StatementNode* body() { return body_; }
	StatementNode* else_body() { return else_body_; }
	
private:
	IfElseNode() {}
	ExpressionNode* condition_;
	StatementNode* body_;
	StatementNode* else_body_;
};

class GotoNode : public StatementNode
{
public:
	static GotoNode* create(const char* target);
	virtual const char* str() const { return "Goto"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
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
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	ExpressionNode* expression() { return expression_; }
	
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
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	VariableNode* target() { return target_; }
	
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
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	
	VariableNode* target() { return target_; }
	ExpressionNode* value() { return value_; }
	
private:
	AssignNode() {}
	VariableNode* target_;
	ExpressionNode* value_;
};

#endif
