#ifndef AST_HPP
#define AST_HPP

#include <iostream>
#include <list>
#include <memory>
#include "ast_visitor.hpp"
#include "scope.hpp"
#include "string_table.hpp"
#include "types.hpp"

struct YYLTYPE;

/* Abstract base nodes */
class AstNode
{
public:
	AstNode();
	virtual ~AstNode();
	virtual void accept(AstVisitor* visitor) = 0;

	YYLTYPE* location() { return location_; }
	Type type() { return type_; }

	void setType(Type type) { type_ = type; }

protected:
	YYLTYPE* location_;
	Type type_;
};

class StatementNode : public AstNode {};
class ExpressionNode : public AstNode {};

/* Top-level nodes */

class ProgramNode : public AstNode
{
public:
	ProgramNode() : scope_(new Scope) {}

	void append(AstNode* child);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::list<std::unique_ptr<AstNode>>& children() const { return children_; }
	Scope* scope() { return scope_.get(); }

private:
	std::list<std::unique_ptr<AstNode>> children_;
	std::unique_ptr<Scope> scope_;
};

class ParamListNode : public AstNode
{
public:
	void append(const char* param);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::list<const char*>& names() const { return names_; }

private:
	std::list<const char*> names_;
};

/* Expression nodes */
class NotNode : public ExpressionNode
{
public:
	NotNode(ExpressionNode* expression) : expression_(expression) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* child() { return expression_.get(); }

private:
	std::unique_ptr<ExpressionNode> expression_;
};

class BinaryOperatorNode : public ExpressionNode
{
public:
	enum Operator {kPlus, kMinus, kTimes, kDivide, kMod};

	BinaryOperatorNode(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: lhs_(lhs), op_(op), rhs_(rhs)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* lhs() { return lhs_.get(); }
	Operator op() { return op_; }
	ExpressionNode* rhs() { return rhs_.get(); }

protected:
	std::unique_ptr<ExpressionNode> lhs_;
	Operator op_;
	std::unique_ptr<ExpressionNode> rhs_;
};

class LogicalNode : public ExpressionNode
{
public:
	enum Operator {kAnd, kOr};

	LogicalNode(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: lhs_(lhs), op_(op), rhs_(rhs)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* lhs() { return lhs_.get(); }
	Operator op() { return op_; }
	ExpressionNode* rhs() { return rhs_.get(); }

protected:
	std::unique_ptr<ExpressionNode> lhs_;
	Operator op_;
	std::unique_ptr<ExpressionNode> rhs_;
};

class ComparisonNode : public ExpressionNode
{
public:
	enum Operator { kGreater, kEqual, kLess, kGreaterOrEqual, kLessOrEqual, kNotEqual };

	ComparisonNode(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: lhs_(lhs), op_(op), rhs_(rhs)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* lhs() { return lhs_.get(); }
	Operator op() { return op_; }
	ExpressionNode* rhs() { return rhs_.get(); }

protected:
	std::unique_ptr<ExpressionNode> lhs_;
	Operator op_;
	std::unique_ptr<ExpressionNode> rhs_;
};

class VariableNode : public ExpressionNode
{
public:
	VariableNode(const char* name) : name_(name), symbol_(nullptr) {}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const char* name() { return name_; }
	const Symbol* symbol() { return symbol_; }

	void attachSymbol(Symbol* symbol) { symbol_ = symbol; }

private:
	const char* name_;
	Symbol* symbol_;
};

class IntNode : public ExpressionNode
{
public:
	IntNode(long value) : value_(value) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	long value() { return value_; }

private:
	long value_;
};

typedef std::list<std::unique_ptr<ExpressionNode>> ArgList;

class FunctionCallNode : public ExpressionNode
{
public:
	FunctionCallNode(const char* target, ArgList* arguments)
	: target_(target)
	{
		arguments_.reset(arguments);
	}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const char* target() { return target_; }
	ArgList& arguments() { return *arguments_.get(); }

private:
	const char* target_;
	std::unique_ptr<ArgList> arguments_;
};

class ReadNode : public ExpressionNode
{
public:
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
};

/* Statement nodes */
class BlockNode : public StatementNode
{
public:
	void append(StatementNode* child);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::list<std::unique_ptr<StatementNode>>& children() const { return children_; }

private:
	std::list<std::unique_ptr<StatementNode>> children_;
};

class IfNode : public StatementNode
{
public:
	IfNode(ExpressionNode* condition, StatementNode* body) : condition_(condition), body_(body) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* condition() { return condition_.get(); }
	StatementNode* body() { return body_.get(); }

private:
	std::unique_ptr<ExpressionNode> condition_;
	std::unique_ptr<StatementNode> body_;
};

class IfElseNode : public StatementNode
{
public:
	IfElseNode(ExpressionNode* condition, StatementNode* body, StatementNode* else_body)
	: condition_(condition), body_(body), else_body_(else_body)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* condition() { return condition_.get(); }
	StatementNode* body() { return body_.get(); }
	StatementNode* else_body() { return else_body_.get(); }

private:
	std::unique_ptr<ExpressionNode> condition_;
	std::unique_ptr<StatementNode> body_;
	std::unique_ptr<StatementNode> else_body_;
};

class PrintNode : public StatementNode
{
public:
	PrintNode(ExpressionNode* expression) : expression_(expression) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* expression() { return expression_.get(); }

private:
	std::unique_ptr<ExpressionNode> expression_;
};

class WhileNode : public StatementNode
{
public:
	WhileNode(ExpressionNode* condition, StatementNode* body) : condition_(condition), body_(body) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* condition() { return condition_.get(); }
	StatementNode* body() { return body_.get(); }

private:
	std::unique_ptr<ExpressionNode> condition_;
	std::unique_ptr<StatementNode> body_;
};

class AssignNode : public StatementNode
{
public:
	AssignNode(const char* target, ExpressionNode* value)
	: target_(target), value_(value), symbol_(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const char* target() { return target_; }
	ExpressionNode* value() { return value_.get(); }
	Symbol* symbol() { return symbol_; }

	void attachSymbol(Symbol* symbol) { symbol_ = symbol; }

private:
	const char* target_;
	std::unique_ptr<ExpressionNode> value_;

	Symbol* symbol_;
};

class FunctionDefNode : public StatementNode
{
public:
	FunctionDefNode(const char* name, StatementNode* body, ParamListNode* params)
	: name_(name), body_(body), params_(params), symbol_(nullptr), scope_(new Scope)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const char* name() { return name_; }
	StatementNode* body() { return body_.get(); }
	const std::list<const char*>& params() { return params_->names(); }

	Scope* scope() { return scope_.get(); }
	void attachSymbol(Symbol* symbol) { symbol_ = symbol; }

private:
	const char* name_;
	std::unique_ptr<StatementNode> body_;
	std::unique_ptr<ParamListNode> params_;

	Symbol* symbol_;
	std::unique_ptr<Scope> scope_;
};

class ReturnNode : public StatementNode
{
public:
	ReturnNode(ExpressionNode* expression) : expression_(expression) {}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* expression() { return expression_.get(); }

private:
	std::unique_ptr<ExpressionNode> expression_;
};

#endif
