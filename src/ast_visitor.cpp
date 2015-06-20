#include <list>
#include "ast.hpp"
#include "ast_visitor.hpp"

void AstVisitor::visit(ProgramNode* node)
{
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
	for (auto& argument : node->arguments)
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

void AstVisitor::visit(ForeverNode* node)
{
	node->body->accept(this);
}

void AstVisitor::visit(AssignNode* node)
{
	node->value->accept(this);
}

void AstVisitor::visit(LetNode* node)
{
	node->value->accept(this);
	if (node->typeName)
		node->typeName->accept(this);
}

void AstVisitor::visit(FunctionDefNode* node)
{
	node->body->accept(this);
	node->typeName->accept(this);
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
	for (auto& member : node->members)
	{
		member->accept(this);
	}
}

void AstVisitor::visit(SwitchNode* node)
{
	node->expr->accept(this);
	for (auto& arm : node->arms)
	{
		arm->accept(this);
	}
}

void AstVisitor::visit(MatchArm* node)
{
	node->body->accept(this);
}

void AstVisitor::visit(ConstructorSpec* node)
{
	for (auto& member : node->members)
	{
		member->accept(this);
	}
}

void AstVisitor::visit(ForeignDeclNode* node)
{
	node->typeName->accept(this);
}

void AstVisitor::visit(MemberDefNode* node)
{
	node->typeName->accept(this);
}

void AstVisitor::visit(DataDeclaration* node)
{
	for (auto& spec : node->constructorSpecs)
	{
		spec->accept(this);
	}
}
