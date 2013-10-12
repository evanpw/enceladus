#include <cassert>
#include <iostream>
#include "ast.hpp"
#include "memory_manager.hpp"
#include "simple.tab.h"
#include "utilities.hpp"

using namespace std;

AstNode::AstNode()
{
	location_ = new YYLTYPE(yylloc);
}

AstNode::~AstNode()
{
	delete location_;
}

void AstNode::show(ostream& out, int depth) const
{
	indent(out, depth);
	out << str() << endl;
}

ProgramNode* ProgramNode::create()
{
	ProgramNode* node = new ProgramNode;
	MemoryManager::addNode(node);

	return node;
}

void ProgramNode::prepend(AstNode* child)
{
	children_.push_back(child);
}

void ProgramNode::show(ostream& out, int depth) const
{
	AstNode::show(out, depth);
	for (list<AstNode*>::const_iterator i = children_.begin(); i != children_.end(); ++i)
	{
		(*i)->show(out, depth + 1);
	}
}

LabelNode* LabelNode::create(const char* name)
{
	LabelNode* node = new LabelNode;
	MemoryManager::addNode(node);

	node->name_ = name;
	node->symbol_ = 0;
	return node;
}

void LabelNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "Label: " << name_ << endl;
}

NotNode* NotNode::create(ExpressionNode* expression)
{
	NotNode* node = new NotNode;
	MemoryManager::addNode(node);

	node->expression_ = expression;
	return node;
}

void NotNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	expression_->show(out, depth + 1);
}

BinaryOperatorNode* BinaryOperatorNode::create(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
{
	BinaryOperatorNode* node = new BinaryOperatorNode;
	MemoryManager::addNode(node);

	node->lhs_ = lhs;
	node->op_ = op;
	node->rhs_ = rhs;
	return node;
}

void BinaryOperatorNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	lhs_->show(out, depth + 1);
	rhs_->show(out, depth + 1);
}

const char* BinaryOperatorNode::str() const
{
	switch (op_)
	{
	case kPlus: return "BinaryOp(+)";
	case kMinus: return "BinaryOp(-)";
	case kTimes: return "BinaryOp(*)";
	case kDivide: return "BinaryOp(/)";
	case kMod: return "BinaryOp(mod)";
	}

	assert(false);
}

LogicalNode* LogicalNode::create(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
{
	LogicalNode* node = new LogicalNode;
	MemoryManager::addNode(node);

	node->lhs_ = lhs;
	node->op_ = op;
	node->rhs_ = rhs;
	return node;
}

void LogicalNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	lhs_->show(out, depth + 1);
	rhs_->show(out, depth + 1);
}

const char* LogicalNode::str() const
{
	switch (op_)
	{
	case kAnd: return "Logical(and)";
	case kOr: return "Logical(or)";
	}

	assert(false);
}

ComparisonNode* ComparisonNode::create(ExpressionNode* lhs, ComparisonNode::Operator op, ExpressionNode* rhs)
{
	ComparisonNode* node = new ComparisonNode;
	MemoryManager::addNode(node);

	node->lhs_ = lhs;
	node->op_ = op;
	node->rhs_ = rhs;
	return node;
}

const char* ComparisonNode::str() const
{
	switch (op_)
	{
	case kGreater: return "Comparison(>)";
	case kLess: return "Comparison(<)";
	case kEqual: return "Comparison(=)";
	case kGreaterOrEqual: return "Comparison(>=)";
	case kLessOrEqual: return "Comparison(<=)";
	case kNotEqual: return "Comparison(!=)";
	}

	assert(false);
}

void ComparisonNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	lhs_->show(out, depth + 1);
	rhs_->show(out, depth + 1);
}

VariableNode* VariableNode::create(const char* name)
{
	VariableNode* node = new VariableNode;
	MemoryManager::addNode(node);

	node->name_ = name;
	node->symbol_ = 0;
	return node;
}

void VariableNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "Variable: " << name_ << endl;
}

IntNode* IntNode::create(long value)
{
	IntNode* node = new IntNode;
	MemoryManager::addNode(node);

	node->value_ = value;
	return node;
}

void IntNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "Int: " << value_ << endl;
}

BlockNode* BlockNode::create()
{
	BlockNode* node = new BlockNode;
	MemoryManager::addNode(node);

	return node;
}

void BlockNode::prepend(StatementNode* child)
{
	children_.push_back(child);
}

void BlockNode::show(ostream& out, int depth) const
{
	AstNode::show(out, depth);
	for (list<StatementNode*>::const_iterator i = children_.begin(); i != children_.end(); ++i)
	{
		(*i)->show(out, depth + 1);
	}
}

IfNode* IfNode::create(ExpressionNode* condition, StatementNode* body)
{
	IfNode* node = new IfNode;
	MemoryManager::addNode(node);

	node->condition_ = condition;
	node->body_ = body;
	return node;
}

void IfNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	condition_->show(out, depth + 1);
	body_->show(out, depth + 1);
}

IfElseNode* IfElseNode::create(ExpressionNode* condition, StatementNode* body, StatementNode* else_body)
{
	IfElseNode* node = new IfElseNode;
	MemoryManager::addNode(node);

	node->condition_ = condition;
	node->body_ = body;
	node->else_body_ = else_body;
	return node;
}

void IfElseNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	condition_->show(out, depth + 1);
	body_->show(out, depth + 1);
	else_body_->show(out, depth + 1);
}

GotoNode* GotoNode::create(const char* target)
{
	GotoNode* node = new GotoNode;
	MemoryManager::addNode(node);

	node->target_ = target;
	return node;
}

void GotoNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "Goto: " << target_ << endl;
}

PrintNode* PrintNode::create(ExpressionNode* expression)
{
	PrintNode* node = new PrintNode;
	MemoryManager::addNode(node);

	node->expression_ = expression;
	return node;
}

void PrintNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	expression_->show(out, depth + 1);
}

ReadNode* ReadNode::create(const char* target)
{
	ReadNode* node = new ReadNode;
	MemoryManager::addNode(node);

	node->target_ = target;
	return node;
}

void ReadNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "Read: " << target_ << endl;
}

WhileNode* WhileNode::create(ExpressionNode* condition, StatementNode* body)
{
	WhileNode* node = new WhileNode;
	MemoryManager::addNode(node);

	node->condition_ = condition;
	node->body_ = body;
	return node;
}

void WhileNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	condition_->show(out, depth + 1);
	body_->show(out, depth + 1);
}

AssignNode* AssignNode::create(const char* target, ExpressionNode* value)
{
	AssignNode* node = new AssignNode;
	MemoryManager::addNode(node);

	node->target_ = target;
	node->value_ = value;
	return node;
}

void AssignNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);

	indent(out, depth + 1);
	out << target_ << endl;

	value_->show(out, depth + 1);
}

FunctionDefNode* FunctionDefNode::create(const char* name, StatementNode* body)
{
	FunctionDefNode* node = new FunctionDefNode;
	MemoryManager::addNode(node);

	node->name_ = name;
	node->body_ = body;
	return node;
}

void FunctionDefNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "Function: " << name_ << endl;
	body_->show(out, depth + 1);
}

FunctionCallNode* FunctionCallNode::create(const char* target)
{
	FunctionCallNode* node = new FunctionCallNode;
	MemoryManager::addNode(node);

	node->target_ = target;
	return node;
}

void FunctionCallNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "FunctionCall: " << target_ << endl;
}

ReturnNode* ReturnNode::create(ExpressionNode* expression)
{
	ReturnNode* node = new ReturnNode;
	MemoryManager::addNode(node);

	node->expression_ = expression;
	return node;
}

void ReturnNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	expression_->show(out, depth + 1);
}
