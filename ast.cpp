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

LabelNode* LabelNode::create(const char* name)
{
	LabelNode* node = new LabelNode;
	MemoryManager::addNode(node);

	node->name_ = name;
	node->symbol_ = 0;
	return node;
}

NotNode* NotNode::create(ExpressionNode* expression)
{
	NotNode* node = new NotNode;
	MemoryManager::addNode(node);

	node->expression_ = expression;
	return node;
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

LogicalNode* LogicalNode::create(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
{
	LogicalNode* node = new LogicalNode;
	MemoryManager::addNode(node);

	node->lhs_ = lhs;
	node->op_ = op;
	node->rhs_ = rhs;
	return node;
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

VariableNode* VariableNode::create(const char* name)
{
	VariableNode* node = new VariableNode;
	MemoryManager::addNode(node);

	node->name_ = name;
	node->symbol_ = 0;
	return node;
}

IntNode* IntNode::create(long value)
{
	IntNode* node = new IntNode;
	MemoryManager::addNode(node);

	node->value_ = value;
	return node;
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

IfNode* IfNode::create(ExpressionNode* condition, StatementNode* body)
{
	IfNode* node = new IfNode;
	MemoryManager::addNode(node);

	node->condition_ = condition;
	node->body_ = body;
	return node;
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

GotoNode* GotoNode::create(const char* target)
{
	GotoNode* node = new GotoNode;
	MemoryManager::addNode(node);

	node->target_ = target;
	return node;
}

PrintNode* PrintNode::create(ExpressionNode* expression)
{
	PrintNode* node = new PrintNode;
	MemoryManager::addNode(node);

	node->expression_ = expression;
	return node;
}

ReadNode* ReadNode::create()
{
	ReadNode* node = new ReadNode;
	MemoryManager::addNode(node);

	return node;
}

WhileNode* WhileNode::create(ExpressionNode* condition, StatementNode* body)
{
	WhileNode* node = new WhileNode;
	MemoryManager::addNode(node);

	node->condition_ = condition;
	node->body_ = body;
	return node;
}

AssignNode* AssignNode::create(const char* target, ExpressionNode* value)
{
	AssignNode* node = new AssignNode;
	MemoryManager::addNode(node);

	node->target_ = target;
	node->value_ = value;
	return node;
}

FunctionDefNode* FunctionDefNode::create(const char* name, StatementNode* body)
{
	FunctionDefNode* node = new FunctionDefNode;
	MemoryManager::addNode(node);

	node->name_ = name;
	node->body_ = body;
	return node;
}

FunctionCallNode* FunctionCallNode::create(const char* target)
{
	FunctionCallNode* node = new FunctionCallNode;
	MemoryManager::addNode(node);

	node->target_ = target;
	return node;
}

ReturnNode* ReturnNode::create(ExpressionNode* expression)
{
	ReturnNode* node = new ReturnNode;
	MemoryManager::addNode(node);

	node->expression_ = expression;
	return node;
}
