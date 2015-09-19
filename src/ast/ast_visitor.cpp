#include "ast/ast_visitor.hpp"
#include "ast.hpp"

#include <list>

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
	node->elseBody->accept(this);
}

void AstVisitor::visit(WhileNode* node)
{
	node->condition->accept(this);
	node->body->accept(this);
}

void AstVisitor::visit(ForeachNode* node)
{
	node->listExpression->accept(this);
	node->body->accept(this);
}

void AstVisitor::visit(ForNode* node)
{
	node->fromExpression->accept(this);
	node->toExpression->accept(this);
	node->body->accept(this);
}

void AstVisitor::visit(ForeverNode* node)
{
	node->body->accept(this);
}

void AstVisitor::visit(AssignNode* node)
{
	node->rhs->accept(this);
}

void AstVisitor::visit(VariableDefNode* node)
{
	node->rhs->accept(this);
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

void AstVisitor::visit(LetNode* node)
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

void AstVisitor::visit(MatchNode* node)
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



void AstVisitor::visit(FunctionDeclNode* node)
{
	node->typeName->accept(this);
}

void AstVisitor::visit(TraitDefNode* node)
{
	for (auto& method : node->methods)
	{
		method->accept(this);
	}
}

void AstVisitor::visit(TraitImplNode* node)
{
	for (auto& method : node->methods)
	{
		method->accept(this);
	}

	node->typeName->accept(this);
}

void AstVisitor::visit(ImplNode* node)
{
	for (auto& method : node->methods)
	{
		method->accept(this);
	}

	node->typeName->accept(this);
}

void AstVisitor::visit(MethodDefNode* node)
{
	node->body->accept(this);
	node->typeName->accept(this);
}