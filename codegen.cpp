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
			for (const std::string& param : paramList)
			{
				if (param == symbol->name)
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

std::vector<std::string> CodeGen::getExterns(ProgramNode* node)
{
	std::vector<std::string> result;

	for (auto& i : node->scope()->symbols())
	{
		const std::string& name = i.first;
		auto& symbol = i.second;

		if (symbol->isExternal)
		{
			result.push_back(name);
		}
	}

	return result;
}

void CodeGen::visit(ProgramNode* node)
{
	out_ << "bits 64" << std::endl;
	out_ << "section .text" << std::endl;
	out_ << "global __main" << std::endl;

	std::vector<std::string> externs = getExterns(node);
	out_ << "extern __read, __print, __cons, __die, __incref, __decref, __decref_no_free" << std::endl;
	if (!externs.empty())
	{
		out_ << "extern _" << externs[0];
		for (size_t i = 1; i < externs.size(); ++i)
		{
			out_ << ", _" << externs[i];
		}
		out_ << std::endl;
	}
	out_ << std::endl;

	out_ << "__main:" << std::endl;
	currentFunction_ = "_main";

	// Recurse to child nodes
	AstVisitor::visit(node);

	out_ << "__end__main:" << std::endl;

	// Clean up all global variables before exiting, just to make valgrind
	// happy
	for (auto& i : topScope()->symbols())
	{
		auto& symbol = i.second;
		if (symbol->kind == kVariable && symbol->type == &Type::List)
		{
			out_ << "\t" << "mov rdi, " << access(symbol.get()) << std::endl;
			out_ << "\t" << "call __decref" << std::endl;
		}
	}

	out_ << "\t" << "ret" << std::endl;

	// All other functions
	for (FunctionDefNode* function : functionDefs_)
	{
		currentFunction_ = function->name();
		out_ << std::endl;
		out_ << "_" << function->name() << ":" << std::endl;
		out_ << "\t" << "push rbp" << std::endl;
		out_ << "\t" << "mov rbp, rsp" << std::endl;

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
		if (locals > 0) out_ << "\t" << "add rsp, -" << (8 * locals) << std::endl;

		// We have to zero out the local variables for the reference counting
		// to work correctly
		out_ << "\t" << "mov rax, 0" << std::endl;
		out_ << "\t" << "mov rcx, " << locals << std::endl;
		out_ << "\t" << "mov rdi, rsp" << std::endl;
		out_ << "\t" << "rep stosq" << std::endl;

		// We gain a reference to all of the parameters passed in
		for (auto& i : function->scope()->symbols())
		{
			auto& symbol = i.second;
			if (symbol->isParam && symbol->type == &Type::List)
			{
				out_ << "\t" << "mov rdi, " << access(symbol.get()) << std::endl;
				out_ << "\t" << "call __incref" << std::endl;
			}
		}

		// Recurse to children
		AstVisitor::visit(function);

		out_ << "__end_" << function->name() << ":" << std::endl;
		out_ << "\t" << "push rax" << std::endl;

		// Preserve the return value from being freed if it happens to be the
		// same as one of the local variables.
		if (function->symbol()->type == &Type::List)
		{
			out_ << "\t" << "mov rdi, rax" << std::endl;
			out_ << "\t" << "call __incref" << std::endl;
		}

		// Going out of scope loses a reference to all of the local variables
		for (auto& i : function->scope()->symbols())
		{
			auto& symbol = i.second;
			if (symbol->type == &Type::List)
			{
				out_ << "\t" << "mov rdi, " << access(symbol.get()) << std::endl;
				out_ << "\t" << "call __decref" << std::endl;
			}
		}

		// But after the function returns, we don't have a reference to the
		// return value, it's just in a temporary. The caller will have to
		// assign it a reference.
		if (function->symbol()->type == &Type::List)
		{
			out_ << "\t" << "mov rdi, qword [rsp]" << std::endl;
			out_ << "\t" << "call __decref_no_free" << std::endl;
		}

		out_ << "\t" << "pop rax" << std::endl;

		out_ << "\t" << "mov rsp, rbp" << std::endl;
		out_ << "\t" << "pop rbp" << std::endl;
		out_ << "\t" << "ret" << std::endl;
	}

	// Declare global variables in the data segment
	out_ << std::endl<< "section .data" << std::endl;
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
	out_ << "\t" << "xor rax, 1" << std::endl;
}

void CodeGen::visit(ConsNode* node)
{
	node->rhs()->accept(this);
	out_ << "\t" << "push rax" << std::endl;

	node->lhs()->accept(this);
	out_ << "\t" << "mov rdi, rax" << std::endl;
	out_ << "\t" << "pop rsi" << std::endl;

	out_ << "\t" << "call __cons" << std::endl;
}

void CodeGen::visit(HeadNode* node)
{
	node->child()->accept(this);

	std::string good = uniqueLabel();

	out_ << "\t" << "cmp rax, 0" << std::endl;
	out_ << "\t" << "jne " << good << std::endl;

	// If the list is null, then fail
	out_ << "\t" << "xor rax, rax" << std::endl; // Not necessary, but good to be explicit
	out_ << "\t" << "call __die" << std::endl;

	out_ << good << ":" << std::endl;
	out_ << "\t" << "mov rax, qword [rax]" << std::endl;
}

void CodeGen::visit(TailNode* node)
{
	node->child()->accept(this);

	std::string good = uniqueLabel();

	out_ << "\t" << "cmp rax, 0" << std::endl;
	out_ << "\t" << "jne " << good << std::endl;

	// If the list is null, then fail
	out_ << "\t" << "mov rax, 1" << std::endl; // Not necessary, but good to be explicit
	out_ << "\t" << "call __die" << std::endl;

	out_ << good << ":" << std::endl;
	out_ << "\t" << "mov rax, qword [rax + 8]" << std::endl;
}

void CodeGen::visit(NullNode* node)
{
	std::string finish = uniqueLabel();

	node->child()->accept(this);
	out_ << "\t" << "cmp rax, 0" << std::endl;
	out_ << "\t" << "je " << finish << std::endl;
	out_ << "\t" << "mov rax, 1" << std::endl;
	out_ << finish << ":" << std::endl;
	out_ << "\t" << "xor rax, 1" << std::endl;
}

void CodeGen::visit(ComparisonNode* node)
{
	node->lhs()->accept(this);
	out_ << "\t" << "push rax" << std::endl;
	node->rhs()->accept(this);
	out_ << "\t" << "cmp qword [rsp], rax" << std::endl;

	std::string trueBranch = uniqueLabel();
	std::string endLabel = uniqueLabel();

	switch(node->op())
	{
		case ComparisonNode::kGreater:
			out_ << "\t" << "jg near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kLess:
			out_ << "\t" << "jl near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kEqual:
			out_ << "\t" << "je near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kGreaterOrEqual:
			out_ << "\t" << "jge near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kLessOrEqual:
			out_ << "\t" << "jle near " << trueBranch << std::endl;
			break;

		case ComparisonNode::kNotEqual:
			out_ << "\t" << "jne near " << trueBranch << std::endl;
			break;

		default: assert(false);

	}

	out_ << "\t" << "mov rax, 0" << std::endl;
	out_ << "\t" << "jmp " << endLabel << std::endl;
	out_ << trueBranch << ":" << std::endl;
	out_ << "\t" << "mov rax, 1" << std::endl;
	out_ << endLabel << ":" << std::endl;
	out_ << "\t" << "pop rbx" << std::endl;
}

void CodeGen::visit(BinaryOperatorNode* node)
{
	node->lhs()->accept(this);
	out_ << "\t" << "push rax" << std::endl;
	node->rhs()->accept(this);

	switch (node->op())
	{
	case BinaryOperatorNode::kPlus:
		out_ << "\t" << "add rax, qword [rsp]" << std::endl;
		break;

	case BinaryOperatorNode::kMinus:
		out_ << "\t" << "xchg rax, qword [rsp]" << std::endl;
		out_ << "\t" << "sub rax, qword [rsp]" << std::endl;
		break;

	case BinaryOperatorNode::kTimes:
		out_ << "\t" << "imul rax, qword [rsp]" << std::endl;
		break;

	case BinaryOperatorNode::kDivide:
		out_ << "\t" << "xchg rax, qword [rsp]" << std::endl;
		out_ << "\t" << "cqo" << std::endl;
		out_ << "\t" << "idiv qword [rsp]" << std::endl;
		break;

	case BinaryOperatorNode::kMod:
		out_ << "\t" << "xchg rax, qword [rsp]" << std::endl;
		out_ << "\t" << "cqo" << std::endl;
		out_ << "\t" << "idiv qword [rsp]" << std::endl;
		out_ << "\t" << "mov rax, rdx" << std::endl;
		break;
	}

	out_ << "\t" << "pop rbx" << std::endl;
}

void CodeGen::visit(LogicalNode* node)
{
	node->lhs()->accept(this);
	out_ << "\t" << "push rax" << std::endl;
	node->rhs()->accept(this);

	switch (node->op())
	{
	case LogicalNode::kAnd:
		out_ << "\t" << "and rax, qword [rsp]" << std::endl;
		break;

	case LogicalNode::kOr:
		out_ << "\t" << "or rax, qword [rsp]" << std::endl;
		break;
	}

	out_ << "\t" << "pop rbx" << std::endl;
}

void CodeGen::visit(VariableNode* node)
{
	out_ << "\t" << "mov rax, " << access(node->symbol()) << std::endl;
}

void CodeGen::visit(IntNode* node)
{
	out_ << "\t" << "mov rax, " << node->value() << std::endl;
}

void CodeGen::visit(NilNode* node)
{
	out_ << "\t" << "mov rax, 0" << std::endl;
}

void CodeGen::visit(BoolNode* node)
{
	if (node->value())
	{
		out_ << "\t" << "mov rax, 1" << std::endl;
	}
	else
	{
		out_ << "\t" << "mov rax, 0" << std::endl;
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

	out_ << "\t" << "cmp rax, 0" << std::endl;
	out_ << "\t" << "je near " << endLabel << std::endl;
	node->body()->accept(this);
	out_ << endLabel << ":" << std::endl;
}

void CodeGen::visit(IfElseNode* node)
{
	node->condition()->accept(this);

	std::string elseLabel = uniqueLabel();
	std::string endLabel = uniqueLabel();

	out_ << "\t" << "cmp rax, 0" << std::endl;
	out_ << "\t" << "je near " << elseLabel << std::endl;
	node->body()->accept(this);
	out_ << "\t" << "jmp " << endLabel << std::endl;
	out_ << elseLabel << ":" << std::endl;
	node->else_body()->accept(this);
	out_ << endLabel << ":" << std::endl;
}

void CodeGen::visit(PrintNode* node)
{
	node->expression()->accept(this);
	out_ << "\t" << "call __print" << std::endl;
}

void CodeGen::visit(ReadNode* node)
{
	out_ << "\t" << "call __read" << std::endl;
}

void CodeGen::visit(WhileNode* node)
{
	std::string beginLabel = uniqueLabel();
	std::string endLabel = uniqueLabel();

	out_ << beginLabel << ":" << std::endl;
	node->condition()->accept(this);

	out_ << "\t" << "cmp rax, 0" << std::endl;
	out_ << "\t" << "je near " << endLabel << std::endl;
	node->body()->accept(this);

	out_ << "\t" << "jmp " << beginLabel << std::endl;
	out_ << endLabel << ":" << std::endl;
}

void CodeGen::visit(AssignNode* node)
{
	node->value()->accept(this);

	// We lose a reference to the original contents, and gain a reference to the
	// new rhs
	if (node->symbol()->type == &Type::List)
	{
		out_ << "\t" << "push rax" << std::endl;

		out_ << "\t" << "mov rdi, rax" << std::endl;
		out_ << "\t" << "call __incref" << std::endl;

		out_ << "\t" << "mov rdi, " << access(node->symbol()) << std::endl;
		out_ << "\t" << "call __decref" << std::endl;

		out_ << "\t" << "pop rax" << std::endl;
	}

	out_ << "\t" << "mov " << access(node->symbol()) << ", rax" << std::endl;
}

void CodeGen::visit(LetNode* node)
{
	node->value()->accept(this);

	// We lose a reference to the original contents, and gain a reference to the
	// new rhs
	if (node->symbol()->type == &Type::List)
	{
		out_ << "\t" << "push rax" << std::endl;

		out_ << "\t" << "mov rdi, rax" << std::endl;
		out_ << "\t" << "call __incref" << std::endl;

		out_ << "\t" << "mov rdi, " << access(node->symbol()) << std::endl;
		out_ << "\t" << "call __decref" << std::endl;

		out_ << "\t" << "pop rax" << std::endl;
	}

	out_ << "\t" << "mov " << access(node->symbol()) << ", rax" << std::endl;
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
		out_ << "\t" << "push rax" << std::endl;
	}

	out_ << "\t" << "call _" << node->target() << std::endl;

	size_t args = node->arguments().size();
	if (args > 0) out_ << "\t" << "add rsp, " << 8 * args << std::endl;
}

void CodeGen::visit(ExternalFunctionCallNode* node)
{
	// Should really be handled in semantic analysis
	assert(node->arguments().size() <= 6);

	for (auto i = node->arguments().rbegin(); i != node->arguments().rend(); ++i)
	{
		(*i)->accept(this);
		out_ << "\t" << "push rax" << std::endl;
	}

	// x86_64 calling convention for C puts the first 6 arguments in registers
	if (node->arguments().size() >= 1) out_ << "\t" << "pop rdi" << std::endl;
	if (node->arguments().size() >= 2) out_ << "\t" << "pop rsi" << std::endl;
	if (node->arguments().size() >= 3) out_ << "\t" << "pop rdx" << std::endl;
	if (node->arguments().size() >= 4) out_ << "\t" << "pop rcx" << std::endl;
	if (node->arguments().size() >= 5) out_ << "\t" << "pop r8" << std::endl;
	if (node->arguments().size() >= 6) out_ << "\t" << "pop r9" << std::endl;

	// Realign the stack to 16 bytes (may not be necessary on all platforms)
    out_ << "\t" << "mov rbx, rsp" << std::endl;
    out_ << "\t" << "and rsp, -16" << std::endl;
    out_ << "\t" << "add rsp, -8" << std::endl;
    out_ << "\t" << "push rbx" << std::endl;

    out_ << "\t" << "call _" << node->target() << std::endl;

    // Undo the stack alignment
    out_ << "\t" << "pop rbx" << std::endl;
    out_ << "\t" << "mov rsp, rbx" << std::endl;
}

void CodeGen::visit(ReturnNode* node)
{
	node->expression()->accept(this);

	out_ << "\t" << "jmp __end_" << currentFunction_ << std::endl;
}
