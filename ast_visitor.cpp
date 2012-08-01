#include <list>
#include "ast.hpp"
#include "ast_visitor.hpp"

void AstVisitor::visit(ProgramNode* node)
{
	for (std::list<AstNode*>::const_iterator i = node->children().begin(); i != node->children().end(); ++i)
	{
		(*i)->accept(this);	
	}
}

void AstVisitor::visit(NotNode* node)
{
	node->child()->accept(this);
}

void AstVisitor::visit(GreaterNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(EqualNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(PlusNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(MinusNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(TimesNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(DivideNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(IfNode* node)
{
	node->condition()->accept(this);
	node->body()->accept(this);
}

void AstVisitor::visit(PrintNode* node)
{
	node->expression()->accept(this);
}

void AstVisitor::visit(ReadNode* node)
{
	node->target()->accept(this);
}

void AstVisitor::visit(AssignNode* node)
{
	node->target()->accept(this);
	node->value()->accept(this);
}