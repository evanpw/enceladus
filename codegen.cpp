#include <cassert>
#include <iostream>
#include <map>
#include "ast.hpp"
#include "codegen.hpp"
#include "scope.hpp"
#include "simple.tab.h"

std::string CodeGen::mangle(const std::string& name)
{
	if (name.find(".") == std::string::npos)
	{
		return name;
	}
	else
	{
		std::string copy = name;
		for (size_t i = 0; i < name.length(); ++i)
		{
			if (copy[i] == '.') copy[i] = '_';
		}

		return copy;
	}
}

std::string CodeGen::access(const VariableSymbol* symbol)
{
	FunctionDefNode* enclosingFunction = symbol->enclosingFunction;

	// Global symbol
	if (symbol->enclosingFunction == nullptr)
	{
		std::stringstream result;
		result << "[rel _" << mangle(symbol->name) << "]";

		return result.str();
	}
	else
	{
		if (symbol->isParam)
		{
			// Parameters should not be assigned a place among the local variables.
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

std::string CodeGen::foreignName(const std::string& name)
{
#ifdef __APPLE__
	std::stringstream ss;
	ss << "_" << name;
	return ss.str();
#else
	return name;
#endif
}

std::vector<std::string> CodeGen::getExterns(ProgramNode* node)
{
	std::vector<std::string> result;

	for (auto& i : node->scope()->symbols())
	{
		const std::string& name = i.first;
		const Symbol* symbol = i.second.get();

		if (symbol->kind != kFunction) continue;

		const FunctionSymbol* functionSymbol = static_cast<const FunctionSymbol*>(symbol);
		if (functionSymbol->isExternal)
		{
			if (!functionSymbol->isForeign)
			{
				result.push_back(name);
			}
			else
			{
				result.push_back(foreignName(name));
			}				
		}
	}

	return result;
}

void CodeGen::createConstructor(ValueConstructor* constructor)
{
	const std::vector<std::shared_ptr<Type>>& memberTypes = constructor->members();

	// For now, every member takes up exactly 8 bytes (either directly or as a pointer).
	// There is one extra qword for the reference count and one for the member
	// counts (pointers and non-pointers)
	size_t size = 8 * (memberTypes.size() + 2);

	out_ << std::endl;
	out_ << "_" << mangle(constructor->name()) << ":" << std::endl;
	out_ << "\t" << "push rbp" << std::endl;
	out_ << "\t" << "mov rbp, rsp" << std::endl;

	// Align stack, allocate, unalign
    out_ << "\t" << "mov rbx, rsp" << std::endl;
    out_ << "\t" << "and rsp, -16" << std::endl;
    out_ << "\t" << "add rsp, -8" << std::endl;
    out_ << "\t" << "push rbx" << std::endl;
    out_ << "\t" << "mov rdi, " << size << std::endl;
	out_ << "\t" << "call " << foreignName("malloc") << std::endl;
    out_ << "\t" << "pop rbx" << std::endl;
    out_ << "\t" << "mov rsp, rbx" << std::endl;

	//// Fill in the members with the constructor arguments

    // Reference count
	out_ << "\t" << "mov qword [rax], 0" << std::endl;

	// Boxed & unboxed member counts
	long memberCount = (constructor->boxedMembers() << 32) + constructor->boxedMembers();
	out_ << "\t" << "mov rbx, qword " << memberCount << std::endl;
	out_ << "\t" << "mov qword [rax + 8], rbx" << std::endl;

    for (size_t i = 0; i < memberTypes.size(); ++i)
    {
    	size_t location = constructor->memberLocations().at(i);

    	out_ << "\t" << "mov rdi, qword [rbp + " << 8 * (i + 2) << "]" << std::endl;
    	out_ << "\t" << "mov qword [rax + " << 8 * (location + 2) << "], rdi" << std::endl;

    	// Increment reference count of non-simple, non-null members
    	if (memberTypes[i]->isBoxed())
    	{
    		std::string skipInc = uniqueLabel();

    		out_ << "\t" << "cmp rdi, 0" << std::endl;
    		out_ << "\t" << "je " << skipInc << std::endl;
    		out_ << "\t" << "add qword [rdi], 1" << std::endl;
    		out_ << skipInc << ":" << std::endl;
    	}
    }

	out_ << "\t" << "mov rsp, rbp" << std::endl;
	out_ << "\t" << "pop rbp" << std::endl;
	out_ << "\t" << "ret" << std::endl;
}

void CodeGen::visit(ProgramNode* node)
{
	out_ << "bits 64" << std::endl;
	out_ << "section .text" << std::endl;
	out_ << "global __main" << std::endl;

	std::vector<std::string> externs = getExterns(node);
	if (!externs.empty())
	{
		out_ << "extern " << mangle(externs[0]);
		for (size_t i = 1; i < externs.size(); ++i)
		{
			out_ << ", " << mangle(externs[i]);
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
		const Symbol* symbol = i.second.get();

		if (symbol->kind != kVariable) continue;

		const VariableSymbol* variableSymbol = static_cast<const VariableSymbol*>(symbol);

		if (variableSymbol->typeScheme->isBoxed())
		{
			out_ << "\t" << "mov rdi, " << access(variableSymbol) << std::endl;
			out_ << "\t" << "call " << foreignName("_decref") << std::endl;
		}
	}

	out_ << "\t" << "ret" << std::endl;

	// All other functions
	for (FunctionDefNode* function : functionDefs_)
	{
		currentFunction_ = function->name();
		out_ << std::endl;
		out_ << "_" << mangle(function->name()) << ":" << std::endl;
		out_ << "\t" << "push rbp" << std::endl;
		out_ << "\t" << "mov rbp, rsp" << std::endl;

		// Assign a location for all of the local variables and parameters.
		int locals = 0;
		for (auto& i : function->scope()->symbols())
		{
			assert(i.second->kind == kVariable); // No locally functions yet

			VariableSymbol* symbol = static_cast<VariableSymbol*>(i.second.get());
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
			assert(i.second->kind == kVariable);

			VariableSymbol* symbol = static_cast<VariableSymbol*>(i.second.get());
			if (symbol->isParam && symbol->typeScheme->isBoxed())
			{
				out_ << "\t" << "mov rdi, " << access(symbol) << std::endl;
				out_ << "\t" << "call " << foreignName("_incref") << std::endl;
			}
		}

		// Recurse to children
		AstVisitor::visit(function);

		out_ << "__end_" << function->name() << ":" << std::endl;
		out_ << "\t" << "push rax" << std::endl;

		assert(function->symbol()->typeScheme->tag() == ttFunction);
		FunctionType* functionType = function->symbol()->typeScheme->type()->get<FunctionType>();

		// Preserve the return value from being freed if it happens to be the
		// same as one of the local variables.
		if (functionType->output()->isBoxed())
		{
			out_ << "\t" << "mov rdi, rax" << std::endl;
			out_ << "\t" << "call " << foreignName("_incref") << std::endl;
		}

		// Going out of scope loses a reference to all of the local variables
		for (auto& i : function->scope()->symbols())
		{
			assert(i.second->kind == kVariable);

			VariableSymbol* symbol = static_cast<VariableSymbol*>(i.second.get());
			if (symbol->typeScheme->isBoxed())
			{
				out_ << "\t" << "mov rdi, " << access(symbol) << std::endl;
				out_ << "\t" << "call " << foreignName("_decref") << std::endl;
			}
		}

		// But after the function returns, we don't have a reference to the
		// return value, it's just in a temporary. The caller will have to
		// assign it a reference.
		if (functionType->output()->isBoxed())
		{
			out_ << "\t" << "mov rdi, qword [rsp]" << std::endl;
			out_ << "\t" << "call " << foreignName("_decrefNoFree") << std::endl;
		}

		out_ << "\t" << "pop rax" << std::endl;

		out_ << "\t" << "mov rsp, rbp" << std::endl;
		out_ << "\t" << "pop rbp" << std::endl;
		out_ << "\t" << "ret" << std::endl;
	}

	for (DataDeclaration* dataDeclaration : dataDeclarations_)
	{
		createConstructor(dataDeclaration->valueConstructor());
	}

	// Declare global variables in the data segment
	out_ << std::endl<< "section .data" << std::endl;
	for (auto& i : topScope()->symbols())
	{
		if (i.second->kind == kVariable)
		{
			out_ << "_" << mangle(i.second->name) << ": dq 0" << std::endl;
		}
	}
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

void CodeGen::visit(NullaryNode* node)
{
	assert(node->symbol()->kind == kVariable || node->symbol()->kind == kFunction);

	if (node->symbol()->kind == kVariable)
	{
		const VariableSymbol* symbol = static_cast<const VariableSymbol*>(node->symbol());
		out_ << "\t" << "mov rax, " << access(symbol) << std::endl;
	}
	else
	{
		const FunctionSymbol* functionSymbol = static_cast<const FunctionSymbol*>(node->symbol());
		if (functionSymbol->isForeign)
		{
			out_ << "\t" << "call " << foreignName(mangle(node->name())) << std::endl;
		}
		else
		{
			out_ << "\t" << "call _" << mangle(node->name()) << std::endl;
		}
	}
}

void CodeGen::visit(IntNode* node)
{
	out_ << "\t" << "mov rax, " << node->value() << std::endl;
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
	if (node->symbol()->typeScheme->isBoxed())
	{
		out_ << "\t" << "push rax" << std::endl;

		out_ << "\t" << "mov rdi, rax" << std::endl;
		out_ << "\t" << "call " << foreignName("_incref") << std::endl;

		out_ << "\t" << "mov rdi, " << access(node->symbol()) << std::endl;
		out_ << "\t" << "call " << foreignName("_decref") << std::endl;

		out_ << "\t" << "pop rax" << std::endl;
	}

	out_ << "\t" << "mov " << access(node->symbol()) << ", rax" << std::endl;
}

void CodeGen::visit(LetNode* node)
{
	node->value()->accept(this);

	// We lose a reference to the original contents, and gain a reference to the
	// new rhs
	if (node->symbol()->typeScheme->isBoxed())
	{
		out_ << "\t" << "push rax" << std::endl;

		out_ << "\t" << "mov rdi, rax" << std::endl;
		out_ << "\t" << "call " << foreignName("_incref") << std::endl;

		out_ << "\t" << "mov rdi, " << access(node->symbol()) << std::endl;
		out_ << "\t" << "call " << foreignName("_decref") << std::endl;

		out_ << "\t" << "pop rax" << std::endl;
	}

	out_ << "\t" << "mov " << access(node->symbol()) << ", rax" << std::endl;
}

void CodeGen::visit(MatchNode* node)
{
	node->body()->accept(this);
	out_ << "\t" << "push rax" << std::endl;

	// Decrement references to the existing variables
	for (size_t i = 0; i < node->symbols().size(); ++i)
	{
		VariableSymbol* member = node->symbols().at(i);

		if (member->typeScheme->isBoxed())
		{
			out_ << "\t" << "mov rdi, " << access(member) << std::endl;
			out_ << "\t" << "call " << foreignName("_decref") << std::endl;
		}
	}

	out_ << "\t" << "pop rax" << std::endl;

	FunctionType* functionType = node->constructorSymbol()->typeScheme->type()->get<FunctionType>();
	auto& constructor = functionType->output()->valueConstructors().front();

	// Copy over each of the members of the constructor pattern
	for (size_t i = 0; i < node->symbols().size(); ++i)
	{
		VariableSymbol* member = node->symbols().at(i);
		size_t location = constructor->memberLocations().at(i);

		out_ << "\t" << "mov rdi, [rax + " << 8 * (location + 2) << "]" << std::endl;
		out_ << "\t" << "mov " << access(member) << ", rdi" << std::endl;
	}

	// Increment references to the new variables
	for (size_t i = 0; i < constructor->boxedMembers(); ++i)
	{
		out_ << "\t" << "mov rdi, [rax + " << 8 * (i + 2) << "]" << std::endl;
		out_ << "\t" << "call " << foreignName("_incref") << std::endl;
	}
}

void CodeGen::visit(FunctionDefNode* node)
{
	functionDefs_.push_back(node);
}

void CodeGen::visit(DataDeclaration* node)
{
	dataDeclarations_.push_back(node);
}

void CodeGen::visit(FunctionCallNode* node)
{
	for (auto i = node->arguments().rbegin(); i != node->arguments().rend(); ++i)
	{
		(*i)->accept(this);
		out_ << "\t" << "push rax" << std::endl;
	}

	if (node->symbol()->isBuiltin)
	{
		if (node->target() == "not")
		{
			out_ << "\t" << "pop rax" << std::endl;
			out_ << "\t" << "xor rax, 1" << std::endl;
		}
		else if (node->target() == "head")
		{
			out_ << "\t" << "pop rax" << std::endl;

			std::string good = uniqueLabel();

			out_ << "\t" << "cmp rax, 0" << std::endl;
			out_ << "\t" << "jne " << good << std::endl;

			// If the list is null, then fail
		    out_ << "\t" << "mov rbx, rsp" << std::endl;
		    out_ << "\t" << "and rsp, -16" << std::endl;
		    out_ << "\t" << "add rsp, -8" << std::endl;
		    out_ << "\t" << "push rbx" << std::endl;
		    out_ << "\t" << "mov rdi, 0" << std::endl;
		    out_ << "\t" << "call _" << foreignName("die") << std::endl;
		    out_ << "\t" << "pop rbx" << std::endl;
		    out_ << "\t" << "mov rsp, rbx" << std::endl;

			out_ << good << ":" << std::endl;
			out_ << "\t" << "mov rax, qword [rax + 24]" << std::endl;
		}
		else if (node->target() == "tail")
		{
			out_ << "\t" << "pop rax" << std::endl;

			std::string good = uniqueLabel();

			out_ << "\t" << "cmp rax, 0" << std::endl;
			out_ << "\t" << "jne " << good << std::endl;

			// If the list is null, then fail
		    out_ << "\t" << "mov rbx, rsp" << std::endl;
		    out_ << "\t" << "and rsp, -16" << std::endl;
		    out_ << "\t" << "add rsp, -8" << std::endl;
		    out_ << "\t" << "push rbx" << std::endl;
		    out_ << "\t" << "mov rdi, 1" << std::endl;
		    out_ << "\t" << "call _" << foreignName("die") << std::endl;
		    out_ << "\t" << "pop rbx" << std::endl;
		    out_ << "\t" << "mov rsp, rbx" << std::endl;

			out_ << good << ":" << std::endl;
			out_ << "\t" << "mov rax, qword [rax + 16]" << std::endl;
		}
		else if (node->target() == "Nil")
		{
			out_ << "\t" << "mov rax, 0" << std::endl;
		}
		else if (node->target() == "null")
		{
			std::string finish = uniqueLabel();
			out_ << "\t" << "pop rax" << std::endl;
			out_ << "\t" << "cmp rax, 0" << std::endl;
			out_ << "\t" << "je " << finish << std::endl;
			out_ << "\t" << "mov rax, 1" << std::endl;
			out_ << finish << ":" << std::endl;
			out_ << "\t" << "xor rax, 1" << std::endl;
		}
		else if (node->target() == "+")
		{
			out_ << "\t" << "pop rbx" << std::endl;
			out_ << "\t" << "pop rax" << std::endl;
			out_ << "\t" << "mov rax, qword [rax + 16]" << std::endl;
			out_ << "\t" << "add rax, qword [rbx + 16]" << std::endl;
		}
		else if (node->target() == "-")
		{
			out_ << "\t" << "pop rbx" << std::endl;
			out_ << "\t" << "pop rax" << std::endl;
			out_ << "\t" << "mov rax, qword [rax + 16]" << std::endl;
			out_ << "\t" << "sub rax, qword [rbx + 16]" << std::endl;
		}
		else if (node->target() == "*")
		{
			out_ << "\t" << "pop rbx" << std::endl;
			out_ << "\t" << "pop rax" << std::endl;
			out_ << "\t" << "mov rax, qword [rax + 16]" << std::endl;
			out_ << "\t" << "imul rax, qword [rbx + 16]" << std::endl;
		}
		else if (node->target() == "/")
		{
			out_ << "\t" << "pop rbx" << std::endl;
			out_ << "\t" << "pop rax" << std::endl;
			out_ << "\t" << "mov rax, qword [rax + 16]" << std::endl;
			out_ << "\t" << "cqo" << std::endl;
			out_ << "\t" << "idiv qword [rbx + 16]" << std::endl;
		}
		else if (node->target() == "%")
		{
			out_ << "\t" << "pop rbx" << std::endl;
			out_ << "\t" << "pop rax" << std::endl;
			out_ << "\t" << "mov rax, qword [rax + 16]" << std::endl;
			out_ << "\t" << "cqo" << std::endl;
			out_ << "\t" << "idiv qword [rbx + 16]" << std::endl;
			out_ << "\t" << "mov rax, rdx" << std::endl;
		}
		else
		{
			assert(false);
		}
	}
	else if (node->symbol()->isForeign)
	{
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

	    out_ << "\t" << "call " << foreignName(mangle(node->target())) << std::endl;

	    // Undo the stack alignment
	    out_ << "\t" << "pop rbx" << std::endl;
	    out_ << "\t" << "mov rsp, rbx" << std::endl;
	}
	else
	{
		out_ << "\t" << "call _" << mangle(node->target()) << std::endl;

		size_t args = node->arguments().size();
		if (args > 0) out_ << "\t" << "add rsp, " << 8 * args << std::endl;
	}
}

void CodeGen::visit(ReturnNode* node)
{
	node->expression()->accept(this);

	out_ << "\t" << "jmp __end_" << currentFunction_ << std::endl;
}
