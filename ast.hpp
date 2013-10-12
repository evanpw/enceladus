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
	enum Operator {kPlus, kMinus, kTimes, kDivide, kMod};
	static BinaryOperatorNode* create(ExpressionNode* lhs, Operator op, ExpressionNode* rhs);
	virtual const char* str() const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	virtual void show(std::ostream& out, int depth) const;

	ExpressionNode* lhs() { return lhs_; }
	Operator op() { return op_; }
	ExpressionNode* rhs() { return rhs_; }

protected:
	BinaryOperatorNode() {}
	ExpressionNode* lhs_;
	Operator op_;
	ExpressionNode* rhs_;
};

class LogicalNode : public ExpressionNode
{
public:
	enum Operator {kAnd, kOr};
	static LogicalNode* create(ExpressionNode* lhs, Operator op, ExpressionNode* rhs);
	virtual const char* str() const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	virtual void show(std::ostream& out, int depth) const;

	ExpressionNode* lhs() { return lhs_; }
	Operator op() { return op_; }
	ExpressionNode* rhs() { return rhs_; }

protected:
	LogicalNode() {}
	ExpressionNode* lhs_;
	Operator op_;
	ExpressionNode* rhs_;
};

class ComparisonNode : public ExpressionNode
{
public:
	enum Operator { kGreater, kEqual, kLess, kGreaterOrEqual, kLessOrEqual, kNotEqual };

	static ComparisonNode* create(ExpressionNode* lhs, Operator op, ExpressionNode* rhs);
	virtual const char* str() const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
	virtual void show(std::ostream& out, int depth) const;

	ExpressionNode* lhs() { return lhs_; }
	Operator op() { return op_; }
	ExpressionNode* rhs() { return rhs_; }

protected:
	ComparisonNode() {}
	ExpressionNode* lhs_;
	Operator op_;
	ExpressionNode* rhs_;
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
	static IntNode* create(long value);
	virtual const char* str() const { return "Int"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	long value() { return value_; }

private:
	IntNode() {}
	long value_;
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
	static ReadNode* create(const char* target);
	virtual const char* str() const { return "Read"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const char* target() { return target_; }

private:
	ReadNode() {}
	const char* target_;
};

class WhileNode : public StatementNode
{
public:
	static WhileNode* create(ExpressionNode* condition, StatementNode* body);
	virtual const char* str() const { return "While"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* condition() { return condition_; }
	StatementNode* body() { return body_; }

private:
	WhileNode() {}
	ExpressionNode* condition_;
	StatementNode* body_;
};

class AssignNode : public StatementNode
{
public:
	static AssignNode* create(const char* target, ExpressionNode* value);
	virtual const char* str() const { return "Assign"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const char* target() { return target_; }
	ExpressionNode* value() { return value_; }

private:
	AssignNode() {}
	const char* target_;
	ExpressionNode* value_;
};

class FunctionDefNode : public StatementNode
{
public:
	static FunctionDefNode* create(const char* name, StatementNode* body);
	virtual const char* str() const { return "FunctionDef"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const char* name() { return name_; }
	StatementNode* body() { return body_; }

	void attachSymbol(Symbol* symbol) { symbol_ = symbol; }

private:
	FunctionDefNode() {}
	const char* name_;
	StatementNode* body_;

	Symbol* symbol_;
};

class FunctionCallNode : public StatementNode
{
public:
	static FunctionCallNode* create(const char* target);
	virtual const char* str() const { return "FunctionCall"; }
	virtual void show(std::ostream& out, int depth) const;
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const char* target() { return target_; }

private:
	FunctionCallNode() {}
	const char* target_;
};

#endif
