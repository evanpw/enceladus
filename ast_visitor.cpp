#include <list>
#include "ast.hpp"
#include "ast_visitor.hpp"

Symbol* AstVisitor::searchScopes(const char* name)
{
	for (auto i = scopes_.rbegin(); i != scopes_.rend(); ++i)
	{
		Symbol* symbol = (*i)->find(name);
		if (symbol != nullptr) return symbol;
	}

	return nullptr;
}

void AstVisitor::visit(ProgramNode* node)
{
	scopes_.push_back(node->scope());

	for (auto& child : node->children())
	{
		child->accept(this);
	}
}

void AstVisitor::visit(NotNode* node)
{
	node->child()->accept(this);
}

void AstVisitor::visit(ComparisonNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(BinaryOperatorNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(LogicalNode* node)
{
	node->lhs()->accept(this);
	node->rhs()->accept(this);
}

void AstVisitor::visit(BlockNode* node)
{
	for (auto& child : node->children())
	{
		child->accept(this);
	}
}

void AstVisitor::visit(IfNode* node)
{
	node->condition()->accept(this);
	node->body()->accept(this);
}

void AstVisitor::visit(IfElseNode* node)
{
	node->condition()->accept(this);
	node->body()->accept(this);
	node->else_body()->accept(this);
}

void AstVisitor::visit(PrintNode* node)
{
	node->expression()->accept(this);
}

void AstVisitor::visit(WhileNode* node)
{
	node->condition()->accept(this);
	node->body()->accept(this);
}

void AstVisitor::visit(AssignNode* node)
{
	node->value()->accept(this);
}

void AstVisitor::visit(FunctionDefNode* node)
{
	enterScope(node->scope());
	node->body()->accept(this);
	exitScope();
}

