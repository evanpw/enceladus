#include <cassert>
#include <iostream>
#include <map>
#include "ast.hpp"
#include "codegen.hpp"
#include "scope.hpp"
#include "simple.tab.h"

std::string CodeGen::access(const Symbol* symbol)
{
	assert(symbol->kind == kVariable);

	FunctionDefNode* enclosingFunction = symbol->enclosingFunction;

	// Global symbol
	if (symbol->enclosingFunction == nullptr)
	{
		std::stringstream result;
		result << "[rel _" << symbol->name << "]";

		return result.str();
	}
	else
	{
		if (symbol->isParam)
		{
			// Symbols should not be assigned a place among the local variables.
			assert(symbol->offset == 0);

			auto paramList = enclosingFunction->params();

			size_t offset = 0;
			for (const char* param : paramList)
			{
				if (strcmp(param, symbol->name) == 0)
				{
					std::stringstream result;
					result << "[rbp + " << 8 * (offset + 2) << "]";
					return result.str();
				}

				++offset;
			}

			// This symbol is supposed to be a parameter, but we can't find it in the
			// parameter list of the function.
			assert(false);
		}
		else
		{
			// This local variable should have been assigned a location on the stack already.
			assert(symbol->offset > 0);

			std::stringstream result;
			result << "[rbp - " << symbol->offset << "]";
			return result.str();
		}
	}
}

void CodeGen::visit(ProgramNode* node)
{
	out_ << "bits 64" << std::endl;
	out_ << "section .text" << std::endl;
	out_ << "global __main" << std::endl;
	out_ << "extern __read, __print, __cons" << std::endl;
	out_ << "__main:" << std::endl;
	currentFunction_ = "_main";

	// Recurse to child nodes
	AstVisitor::visit(node);

	out_ << "__end__main:" << std::endl;
	out_ << "ret" << std::endl;

	// All other functions
	for (FunctionDefNode* function : functionDefs_)
	{
		currentFunction_ = function->name();
		out_ << "_" << function->name() << ":" << std::endl;
		out_ << "push rbp" << std::endl;
		out_ << "mov rbp, rsp" << std::endl;

		// Assign a location for all of the local variables and parameters.
		int locals = 0;
		for (auto& i : function->scope()->symbols())
		{
			auto& symbol = i.second;
			if (!symbol->isParam)
			{
				symbol->offset = 8 * (locals + 1);
				++locals;
			}
		}
		if (locals > 0) out_ << "add rsp, -" << (8 * locals) << std::endl;

		// Recurse to children
		AstVisitor::visit(function);

		out_ << "__end_" << function->name() << ":" << std::endl;
		out_ << "mov rsp, rbp" << std::endl;
		out_ << "pop rbp" << std::endl;
		out_ << "ret" << std::endl;
	}

	// Declare global variables in the data segment
	out_ << "section .data" << std::endl;
	for (auto& i : topScope()->symbols())
	{
		if (i.second->kind == kVariable)
		{
			out_ << "_" << i.second->name << ": dq 0" << std::endl;
		}
	}
}

void CodeGen::visit(NotNode* node)
{
	// Load the value of the child expression in rax
	node->child()->accept(this);
	out_ << "xor rax, 1" << std::endl;
}

void CodeGen::visit(ConsNode* node)
{
	node->lhs()->accept(this);
	out_ << "push rax" << std::endl;

	node->rhs()->accept(this);
	out_ << "mov rsi, rax" << std::endl;
	out_ << "pop rdi" << std::endl;

	out_ << "call __cons" << std::endl;
}

void CodeGen::visit(HeadNode* node)
{
	node->child()->accept(this);
	out_ << "mov rax, qword [rax]" << std::endl;
}

void CodeGen::visit(TailNode* node)
{
	node->child()->accept(this);
	out_ << "mov rax, qword [rax + 8]" << std::endl;
}

void CodeGen::visit(NullNode* node)
{
	std::string finish = uniqueLabel();

	node->child()->accept(this);
	out_ << "cmp rax, 0" << std::endl;
	out_ << "je " << finish << std::endl;
	out_ << "mov rax, 1" << std::endl;
	out_ << finish << ":" << std::endl;
	out_ << "xor rax, 1" << std::endl;
}

void CodeGen::visit(ComparisonNode* node)
{
	node->lhs()->accept(this);
	out_ << "push rax" << std::endl;
	node->rhs()->accept(this);
	out_ << "cmp qword [rsp], rax" << std::endl;

	std::string trueBranch = uniqueLabel();
	std::string endLabel = uniqueLabel();

	switch(node->op())
	{
		case ComparisonNode::kGreater:
			out_ << "jg near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kLess:
			out_ << "jl near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kEqual:
			out_ << "je near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kGreaterOrEqual:
			out_ << "jge near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kLessOrEqual:
			out_ << "jle near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kNotEqual:
			out_ << "jne near " << trueBranch << std::endl;
			break;

		default: assert(false);

	}

	out_ << "mov rax, 0" << std::endl;
	out_ << "jmp " << endLabel << std::endl;
	out_ << trueBranch << ":" << std::endl;
	out_ << "mov rax, 1" << std::endl;
	out_ << endLabel << ":" << std::endl;
	out_ << "pop rbx" << std::endl;
}

void CodeGen::visit(BinaryOperatorNode* node)
{
	node->lhs()->accept(this);
	out_ << "push rax" << std::endl;
	node->rhs()->accept(this);

	switch (node->op())
	{
	case BinaryOperatorNode::kPlus:
		out_ << "add rax, qword [rsp]" << std::endl;
		break;

	case BinaryOperatorNode::kMinus:
		out_ << "xchg rax, qword [rsp]" << std::endl;
		out_ << "sub rax, qword [rsp]" << std::endl;
		break;

	case BinaryOperatorNode::kTimes:
		out_ << "imul rax, qword [rsp]" << std::endl;
		break;

	case BinaryOperatorNode::kDivide:
		out_ << "xchg rax, qword [rsp]" << std::endl;
		out_ << "cqo" << std::endl;
		out_ << "idiv qword [rsp]" << std::endl;
		break;

	case BinaryOperatorNode::kMod:
		out_ << "xchg rax, qword [rsp]" << std::endl;
		out_ << "cqo" << std::endl;
		out_ << "idiv qword [rsp]" << std::endl;
		out_ << "mov rax, rdx" << std::endl;
		break;
	}

	out_ << "pop rbx" << std::endl;
}

void CodeGen::visit(LogicalNode* node)
{
	node->lhs()->accept(this);
	out_ << "push rax" << std::endl;
	node->rhs()->accept(this);

	switch (node->op())
	{
	case LogicalNode::kAnd:
		out_ << "and rax, qword [rsp]" << std::endl;
		break;

	case LogicalNode::kOr:
		out_ << "or rax, qword [rsp]" << std::endl;
		break;
	}

	out_ << "pop rbx" << std::endl;
}

void CodeGen::visit(VariableNode* node)
{
	out_ << "mov rax, " << access(node->symbol()) << std::endl;
}

void CodeGen::visit(IntNode* node)
{
	out_ << "mov rax, " << node->value() << std::endl;
}

void CodeGen::visit(NilNode* node)
{
	out_ << "mov rax, 0" << std::endl;
}

void CodeGen::visit(BoolNode* node)
{
	if (node->value())
	{
		out_ << "mov rax, 1" << std::endl;
	}
	else
	{
		out_ << "mov rax, 0" << std::endl;
	}
}

void CodeGen::visit(BlockNode* node)
{
	for (auto& child : node->children())
	{
		child->accept(this);
	}
}

void CodeGen::visit(IfNode* node)
{
	node->condition()->accept(this);

	std::string endLabel = uniqueLabel();

	out_ << "cmp rax, 0" << std::endl;
	out_ << "je near " << endLabel << std::endl;
	node->body()->accept(this);
	out_ << endLabel << ":" << std::endl;
}

void CodeGen::visit(IfElseNode* node)
{
	node->condition()->accept(this);

	std::string elseLabel = uniqueLabel();
	std::string endLabel = uniqueLabel();

	out_ << "cmp rax, 0" << std::endl;
	out_ << "je near " << elseLabel << std::endl;
	node->body()->accept(this);
	out_ << "jmp " << endLabel << std::endl;
	out_ << elseLabel << ":" << std::endl;
	node->else_body()->accept(this);
	out_ << endLabel << ":" << std::endl;
}

void CodeGen::visit(PrintNode* node)
{
	node->expression()->accept(this);
	out_ << "call __print" << std::endl;
}

void CodeGen::visit(ReadNode* node)
{
	out_ << "call __read" << std::endl;
}

void CodeGen::visit(WhileNode* node)
{
	std::string beginLabel = uniqueLabel();
	std::string endLabel = uniqueLabel();

	out_ << beginLabel << ":" << std::endl;
	node->condition()->accept(this);

	out_ << "cmp rax, 0" << std::endl;
	out_ << "je near " << endLabel << std::endl;
	node->body()->accept(this);

	out_ << "jmp " << beginLabel << std::endl;
	out_ << endLabel << ":" << std::endl;
}

void CodeGen::visit(AssignNode* node)
{
	node->value()->accept(this);
	out_ << "mov " << access(node->symbol()) << ", rax" << std::endl;
}

void CodeGen::visit(LetNode* node)
{
	node->value()->accept(this);
	out_ << "mov " << access(node->symbol()) << ", rax" << std::endl;
}

void CodeGen::visit(FunctionDefNode* node)
{
	functionDefs_.push_back(node);
}

void CodeGen::visit(FunctionCallNode* node)
{
	for (auto i = node->arguments().rbegin(); i != node->arguments().rend(); ++i)
	{
		(*i)->accept(this);
		out_ << "push rax" << std::endl;
	}

	out_ << "call _" << node->target() << std::endl;

	size_t args = node->arguments().size();
	if (args > 0) out_ << "add rsp, " << 8 * args << std::endl;
}

void CodeGen::visit(ReturnNode* node)
{
	node->expression()->accept(this);
	out_ << "jmp __end_" << currentFunction_ << std::endl;
}
