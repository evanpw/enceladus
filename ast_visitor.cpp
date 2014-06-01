#include <list>
#include "ast.hpp"
#include "ast_visitor.hpp"

Symbol* AstVisitor::searchScopes(const std::string& name)
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
	typeTable_ = node->typeTable.get();

	scopes_.push_back(node->scope.get());

	for (auto& child : node->children)
	{
		child->accept(this);
	}
}

void AstVisitor::visit(ComparisonNode* node)
{
	node->lhs->accept(this);
	node->rhs->accept(this);
}

void AstVisitor::visit(LogicalNode* node)
{
	node->lhs->accept(this);
	node->rhs->accept(this);
}

void AstVisitor::visit(BlockNode* node)
{
	for (auto& child : node->children)
	{
		child->accept(this);
	}
}

void AstVisitor::visit(FunctionCallNode* node)
{
	for (auto& argument : *node->arguments)
	{
		argument->accept(this);
	}
}

void AstVisitor::visit(IfNode* node)
{
	node->condition->accept(this);
	node->body->accept(this);
}

void AstVisitor::visit(IfElseNode* node)
{
	node->condition->accept(this);
	node->body->accept(this);
	node->else_body->accept(this);
}

void AstVisitor::visit(WhileNode* node)
{
	node->condition->accept(this);
	node->body->accept(this);
}

void AstVisitor::visit(AssignNode* node)
{
	node->value->accept(this);
}

void AstVisitor::visit(LetNode* node)
{
	node->value->accept(this);
}

void AstVisitor::visit(FunctionDefNode* node)
{
	enterScope(node->scope.get());
	node->body->accept(this);
	exitScope();
}

void AstVisitor::visit(ReturnNode* node)
{
	node->expression->accept(this);
}

void AstVisitor::visit(MatchNode* node)
{
	node->body->accept(this);
}

void AstVisitor::visit(StructDefNode* node)
{
	for (auto& member : *node->members)
	{
		member->accept(this);
	}
}
