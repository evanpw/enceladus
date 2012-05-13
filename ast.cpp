#include <iostream>
#include "ast.hpp"
#include "memory_manager.hpp"
#include "utilities.hpp"

using namespace std;

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

LabelNode* LabelNode::create(IdentNode* name)
{
	LabelNode* node = new LabelNode;
	MemoryManager::addNode(node);
	
	node->name_ = name;
	return node;
}

void LabelNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	name_->show(out, depth + 1);
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

GotoNode* GotoNode::create(IdentNode* target)
{
	GotoNode* node = new GotoNode;
	MemoryManager::addNode(node);
	
	node->target_ = target;
	return node;
}

void GotoNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	target_->show(out, depth + 1);
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

ReadNode* ReadNode::create(IdentNode* target)
{
	ReadNode* node = new ReadNode;
	MemoryManager::addNode(node);
	
	node->target_ = target;
	return node;
}

void ReadNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	target_->show(out, depth + 1);
}

AssignNode* AssignNode::create(IdentNode* target, ExpressionNode* value)
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
	
	target_->show(out, depth + 1); 
	value_->show(out, depth + 1);	
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

void BinaryOperatorNode::show(std::ostream& out, int depth) const
{
	AstNode::show(out, depth);
	lhs_->show(out, depth + 1);
	rhs_->show(out, depth + 1);	
}

GreaterNode* GreaterNode::create(ExpressionNode* lhs, ExpressionNode* rhs)
{
	GreaterNode* node = new GreaterNode;
	MemoryManager::addNode(node);
	
	node->lhs_ = lhs;
	node->rhs_ = rhs;
	return node;
}

EqualNode* EqualNode::create(ExpressionNode* lhs, ExpressionNode* rhs)
{
	EqualNode* node = new EqualNode;
	MemoryManager::addNode(node);
	
	node->lhs_ = lhs;
	node->rhs_ = rhs;
	return node;
}

PlusNode* PlusNode::create(ExpressionNode* lhs, ExpressionNode* rhs)
{
	PlusNode* node = new PlusNode;
	MemoryManager::addNode(node);
	
	node->lhs_ = lhs;
	node->rhs_ = rhs;
	return node;
}

MinusNode* MinusNode::create(ExpressionNode* lhs, ExpressionNode* rhs)
{
	MinusNode* node = new MinusNode;
	MemoryManager::addNode(node);
	
	node->lhs_ = lhs;
	node->rhs_ = rhs;
	return node;
}

TimesNode* TimesNode::create(ExpressionNode* lhs, ExpressionNode* rhs)
{
	TimesNode* node = new TimesNode;
	MemoryManager::addNode(node);
	
	node->lhs_ = lhs;
	node->rhs_ = rhs;
	return node;
}

DivideNode* DivideNode::create(ExpressionNode* lhs, ExpressionNode* rhs)
{
	DivideNode* node = new DivideNode;
	MemoryManager::addNode(node);
	
	node->lhs_ = lhs;
	node->rhs_ = rhs;
	return node;
}

IdentNode* IdentNode::create(const char* symbol)
{
	IdentNode* node = new IdentNode;
	MemoryManager::addNode(node);
	
	node->symbol_ = symbol;
	return node;
}

void IdentNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "Ident: " << symbol_ << endl;
}

IntNode* IntNode::create(const char* symbol)
{
	IntNode* node = new IntNode;
	MemoryManager::addNode(node);
	
	node->symbol_ = symbol;
	return node;
}

void IntNode::show(std::ostream& out, int depth) const
{
	indent(out, depth);
	out << "Int: " << symbol_ << endl;
}