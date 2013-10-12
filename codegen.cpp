#include <iostream>
#include <map>
#include "codegen.hpp"
#include "symbol_table.hpp"

void CodeGen::visit(ProgramNode* node)
{
	out_ << "bits 64" << std::endl;
	out_ << "section .text" << std::endl;
	out_ << "global __main" << std::endl;
	out_ << "extern __read, __print, __exit" << std::endl;
	out_ << "__main:" << std::endl;

	// Main function
	for (std::list<AstNode*>::const_iterator i = node->children().begin(); i != node->children().end(); ++i)
	{
		(*i)->accept(this);
	}

	out_ << "ret" << std::endl;

	// All other functions
	for (FunctionDefNode* node : functionDefs_)
	{
		out_ << "_" << node->name() << ":" << std::endl;
		node->body()->accept(this);
		out_ << "ret" << std::endl;
	}

	out_ << "section .data" << std::endl;
	const std::map<const char*, Symbol*>& symbols = SymbolTable::symbols();
	std::map<const char*, Symbol*>::const_iterator i;
	for (i = symbols.begin(); i != symbols.end(); ++i)
	{
		if (i->second->type == kVariable)
		{
			out_ << "_" << i->second->name << ": dq 0" << std::endl;
		}
	}
}

void CodeGen::visit(LabelNode* node)
{
	out_ << "_" << node->name() << ":" << std::endl;
}

void CodeGen::visit(NotNode* node)
{
	// Load the value of the child expression in eax
	node->child()->accept(this);
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
	out_ << "mov rax, [rel _" << node->name() << "]" << std::endl;
}

void CodeGen::visit(IntNode* node)
{
	out_ << "mov rax, " << node->value() << std::endl;
}

void CodeGen::visit(BlockNode* node)
{
	for (std::list<StatementNode*>::const_iterator i = node->children().begin(); i != node->children().end(); ++i)
	{
		(*i)->accept(this);
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

void CodeGen::visit(GotoNode* node)
{
	out_ << "jmp _" << node->target() << std::endl;
}

void CodeGen::visit(PrintNode* node)
{
	node->expression()->accept(this);
	out_ << "call __print" << std::endl;
}

void CodeGen::visit(ReadNode* node)
{
	out_ << "call __read" << std::endl;
	out_ << "mov [rel _" << node->target() << "], rax" << std::endl;
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
	out_ << "mov [rel _" << node->target() << "], rax" << std::endl;
}

void CodeGen::visit(FunctionDefNode* node)
{
	functionDefs_.push_back(node);
}

void CodeGen::visit(FunctionCallNode* node)
{
	out_ << "call _" << node->target() << std::endl;
}
