#include <cassert>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <map>
#include "ast.hpp"
#include "codegen.hpp"
#include "library.h"
#include "scope.hpp"
#include "parser.hpp"

#define EMIT_BLANK() out_ << std::endl;
#define EMIT_LEFT(x) out_ << x << std::endl;
#define EMIT_LABEL(x) out_ << x << ":" << std::endl;
#define EMIT(x) out_ << "\t" << x << std::endl;
#define EMIT_MALLOC(size) \
    EMIT("mov rbx, rsp"); \
    EMIT("and rsp, -16"); \
    EMIT("add rsp, -8"); \
    EMIT("push rbx"); \
    EMIT("mov rdi, " << size); \
	EMIT("call " << foreignName("malloc")); \
    EMIT("pop rbx"); \
    EMIT("mov rsp, rbx");

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

void CodeGen::getAddress(AssignableNode* node, const std::string& dest)
{
	VariableNode* variableNode = dynamic_cast<VariableNode*>(node);
	assert(variableNode);
	EMIT("lea " << dest << ", " << access(*variableNode->symbol));
}

std::string CodeGen::access(const Symbol& symbol)
{
	assert(symbol.kind == kVariable);

	// Global symbol
	if (symbol.enclosingFunction == nullptr)
	{
		std::stringstream result;
		result << "[rel _" << mangle(symbol.name) << "]";

		return result.str();
	}
	else
	{
		if (symbol.asVariable()->isParam)
		{
			// Parameters should not be assigned a place among the local variables.
			assert(symbol.asVariable()->offset >= 0);

			std::stringstream result;
			result << "[rbp + " << 8 * (2 + symbol.asVariable()->offset) << "]";
			return result.str();
		}
		else
		{
			// This local variable should have been assigned a location on the stack already.
			assert(symbol.asVariable()->offset > 0);

			std::stringstream result;
			result << "[rbp - " << symbol.asVariable()->offset << "]";
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

	for (auto& i : node->scope->symbols.symbols)
	{
		const std::string& name = i.first;
		const Symbol* symbol = i.second.get();

		if (symbol->kind != kFunction) continue;

		if (symbol->asFunction()->isExternal)
		{
			if (!symbol->asFunction()->isForeign)
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
	//std::cerr << "createConstructor: " << constructor->name() << std::endl;

	const std::vector<ValueConstructor::MemberDesc> members = constructor->members();

	// For now, every member takes up exactly 8 bytes (either directly or as a pointer).
	size_t size = sizeof(SplObject) + 8 * members.size();

	EMIT_BLANK();
	EMIT_LABEL("_" << mangle(constructor->name()));
	EMIT("push rbp");
	EMIT("mov rbp, rsp");

    EMIT_MALLOC(size);

	//// Fill in the members with the constructor arguments

    // Reference count
	EMIT("mov qword [rax], 0");

	// Boxed & unboxed member counts
	long memberCount = (constructor->boxedMembers() << 32) + constructor->unboxedMembers();
	EMIT("mov rbx, qword " << memberCount);
	EMIT("mov qword [rax + 8], rbx");

    for (size_t i = 0; i < members.size(); ++i)
    {
    	auto& member = members[i];
    	size_t location = member.location;

    	EMIT("mov rdi, qword [rbp + " << 8 * (2 + i) << "]");
    	EMIT("mov qword [rax + " << sizeof(SplObject) + 8 * location << "], rdi");

    	// Increment reference count of non-simple, non-null members
    	if (member.type->isBoxed())
    	{
    		EMIT("push rax");
			EMIT("call " << foreignName("_incref"));
			EMIT("pop rax");
    	}
    }

	EMIT("mov rsp, rbp");
	EMIT("pop rbp");
	EMIT("ret");
}

void CodeGen::visit(ProgramNode* node)
{
	EMIT_LEFT("bits 64");
	EMIT_LEFT("section .text");
	EMIT_LEFT("global __main");

	std::vector<std::string> externs = getExterns(node);
	if (!externs.empty())
	{
		EMIT_LEFT("extern " << mangle(externs[0]));
		for (size_t i = 1; i < externs.size(); ++i)
		{
			EMIT_LEFT("extern " << mangle(externs[i]));
		}
		EMIT_BLANK();
	}
	EMIT_BLANK();

	EMIT_LABEL("__main");
	currentFunction_ = "_main";

	// Recurse to child nodes
	AstVisitor::visit(node);

	EMIT_LABEL("__end__main");

	// Clean up all global variables before exiting, just to make valgrind
	// happy
	for (auto& i : node->scope->symbols.symbols)
	{
		const Symbol& symbol = *i.second;

		if (symbol.kind != kVariable) continue;

		if (symbol.typeScheme->isBoxed())
		{
			EMIT("mov rdi, " << access(symbol));
			EMIT("call " << foreignName("_decref"));
		}
	}

	EMIT("ret");

	// All other functions
	while (!referencedFunctions_.empty())
	{
		FunctionDefNode* function = *(referencedFunctions_.begin());
		referencedFunctions_.erase(referencedFunctions_.begin());
		visitedFunctions_.insert(function);

		currentFunction_ = function->name;
		EMIT_BLANK();
		EMIT_LABEL("_" << mangle(function->name));
		EMIT("push rbp");
		EMIT("mov rbp, rsp");

		// Assign a location for all of the local variables and parameters.
		int locals = 0;
		for (auto& i : function->scope->symbols.symbols)
		{
			Symbol& symbol = *i.second;
			assert(symbol.kind == kVariable); // No locally functions yet

			if (!symbol.asVariable()->isParam)
			{
				symbol.asVariable()->offset = 8 * (locals + 1);
				++locals;
			}
		}
		if (locals > 0) EMIT("add rsp, -" << (8 * locals));

		// We have to zero out the local variables for the reference counting
		// to work correctly
		EMIT("mov rax, 0");
		EMIT("mov rcx, " << locals);
		EMIT("mov rdi, rsp");
		EMIT("rep stosq");

		// We gain a reference to all of the parameters passed in
		for (auto& i : function->scope->symbols.symbols)
		{
			Symbol& symbol = *i.second;
			assert(symbol.kind == kVariable);

			if (symbol.asVariable()->isParam && symbol.typeScheme->isBoxed())
			{
				EMIT("mov rdi, " << access(symbol));
				EMIT("call " << foreignName("_incref"));
			}
		}

		// Recurse to children
		AstVisitor::visit(function);

		EMIT_LABEL("__end_" << function->name);
		EMIT("push rax");

		assert(function->symbol->typeScheme->tag() == ttFunction);
		FunctionType* functionType = function->symbol->typeScheme->type()->get<FunctionType>();

		// Preserve the return value from being freed if it happens to be the
		// same as one of the local variables.
		if (functionType->output()->isBoxed())
		{
			EMIT("mov rdi, rax");
			EMIT("call " << foreignName("_incref"));
		}

		// Going out of scope loses a reference to all of the local variables
		for (auto& i : function->scope->symbols.symbols)
		{
			const Symbol& symbol = *i.second;
			assert(symbol.kind == kVariable);

			if (symbol.typeScheme->isBoxed())
			{
				EMIT("mov rdi, " << access(symbol));
				EMIT("call " << foreignName("_decref"));
			}
		}

		// But after the function returns, we don't have a reference to the
		// return value, it's just in a temporary. The caller will have to
		// assign it a reference.
		if (functionType->output()->isBoxed())
		{
			EMIT("mov rdi, qword [rsp]");
			EMIT("call " << foreignName("_decrefNoFree"));
		}

		EMIT("pop rax");

		EMIT("mov rsp, rbp");
		EMIT("pop rbp");
		EMIT("ret");
	}

	for (DataDeclaration* dataDeclaration : dataDeclarations_)
	{
		createConstructor(dataDeclaration->valueConstructor);
	}

	for (StructDefNode* structDef : structDeclarations_)
	{
		createConstructor(structDef->valueConstructor);
	}

	// Declare global variables and string literalsin the data segment
	EMIT_BLANK();
	EMIT_LEFT("section .data");
	for (auto& i : node->scope->symbols.symbols)
	{
		if (i.second->kind == kVariable)
		{
			EMIT_LEFT("_" << mangle(i.second->name) << ": dq 0");
		}
	}
}

void CodeGen::visit(ComparisonNode* node)
{
	node->lhs->accept(this);
	EMIT("push rax");
	node->rhs->accept(this);
	EMIT("cmp qword [rsp], rax");

	std::string trueBranch = uniqueLabel();
	std::string endLabel = uniqueLabel();

	switch(node->op)
	{
		case ComparisonNode::kGreater:
			EMIT("jg near " << trueBranch);
			break;

		case ComparisonNode::kLess:
			EMIT("jl near " << trueBranch);
			break;

		case ComparisonNode::kEqual:
			EMIT("je near " << trueBranch);
			break;

		case ComparisonNode::kGreaterOrEqual:
			EMIT("jge near " << trueBranch);
			break;

		case ComparisonNode::kLessOrEqual:
			EMIT("jle near " << trueBranch);
			break;

		case ComparisonNode::kNotEqual:
			EMIT("jne near " << trueBranch);
			break;

		default: assert(false);

	}

	EMIT("mov rax, 01b");
	EMIT("jmp " << endLabel);
	EMIT_LABEL(trueBranch);
	EMIT("mov rax, 11b");
	EMIT_LABEL(endLabel);
	EMIT("pop rbx");
}

void CodeGen::visit(LogicalNode* node)
{
	node->lhs->accept(this);
	EMIT("push rax");
	node->rhs->accept(this);

	switch (node->op)
	{
	case LogicalNode::kAnd:
		EMIT("and rax, qword [rsp]");
		break;

	case LogicalNode::kOr:
		EMIT("or rax, qword [rsp]");
		break;
	}

	EMIT("pop rbx");
}

void CodeGen::visit(NullaryNode* node)
{
	assert(node->symbol->kind == kVariable || node->symbol->kind == kFunction);

	if (node->symbol->kind == kVariable)
	{
		EMIT("mov rax, " << access(*node->symbol));
	}
	else
	{
		if (node->symbol->asFunction()->isForeign)
		{
			// Realign the stack to 16 bytes (may not be necessary on all platforms)
		    EMIT("mov rbx, rsp");
		    EMIT("and rsp, -16");
		    EMIT("add rsp, -8");
		    EMIT("push rbx");

		    EMIT("call " << foreignName(mangle(node->name)));

		    // Undo the stack alignment
		    EMIT("pop rbx");
		    EMIT("mov rsp, rbx");
		}
		else
		{
			if (node->symbol->asFunction()->definition &&
				visitedFunctions_.find(node->symbol->asFunction()->definition) == visitedFunctions_.end())
			{
				referencedFunctions_.insert(node->symbol->asFunction()->definition);
			}

			if (node->type->tag() != ttFunction)
			{
				EMIT("call _" << mangle(node->name));
			}
			else
			{
				// If the function is not completely applied, then create a closure
				size_t size = sizeof(SplObject) + 8;
				EMIT_MALLOC(size);

			    // Reference count
				EMIT("mov qword [rax], 0");

				// 0 boxed members, 1 unboxed
				EMIT("mov qword [rax + 8], 1");

				// Address of the function as an unboxed member
				EMIT("mov rbx, _" << mangle(node->name));
				EMIT("mov qword [rax + " << sizeof(SplObject) << "], rbx");
			}
		}
	}
}

void CodeGen::visit(IntNode* node)
{
	EMIT("mov rax, " << (2 * node->value + 1));
}

void CodeGen::visit(BoolNode* node)
{
	if (node->value)
	{
		EMIT("mov rax, 11b");
	}
	else
	{
		EMIT("mov rax, 01b");
	}
}

void CodeGen::visit(BlockNode* node)
{
	for (auto& child : node->children)
	{
		child->accept(this);
	}
}

void CodeGen::visit(IfNode* node)
{
	node->condition->accept(this);

	std::string endLabel = uniqueLabel();

	EMIT("and rax, 10b");
	EMIT("jz near " << endLabel);
	node->body->accept(this);
	EMIT_LABEL(endLabel);
}

void CodeGen::visit(IfElseNode* node)
{
	node->condition->accept(this);

	std::string elseLabel = uniqueLabel();
	std::string endLabel = uniqueLabel();

	EMIT("and rax, 10b");
	EMIT("jz near " << elseLabel);
	node->body->accept(this);
	EMIT("jmp " << endLabel);
	EMIT_LABEL(elseLabel);
	node->else_body->accept(this);
	EMIT_LABEL(endLabel);
}

void CodeGen::visit(WhileNode* node)
{
	std::string beginLabel = uniqueLabel();
	std::string endLabel = uniqueLabel();

	EMIT_LABEL(beginLabel);
	node->condition->accept(this);

	std::string prevLoopEnd = currentLoopEnd_;
	currentLoopEnd_ = endLabel;

	EMIT("and rax, 10b");
	EMIT("jz near " << endLabel);
	node->body->accept(this);

	currentLoopEnd_ = prevLoopEnd;

	EMIT("jmp " << beginLabel);
	EMIT_LABEL(endLabel);
}

void CodeGen::visit(BreakNode* node)
{
	EMIT("jmp " << currentLoopEnd_);
}

void CodeGen::visit(AssignNode* node)
{
	// Do NOT recurse into the target node, we will instead take its address
	// node->target->accept(this);

	node->value->accept(this);

	// We lose a reference to the original contents, and gain a reference to the
	// new rhs
	if (node->target->type->isBoxed())
	{
		EMIT("push rax");

		EMIT("mov rdi, rax");
		EMIT("call " << foreignName("_incref"));

		getAddress(node->target.get(), "rdi");
		EMIT("mov qword [rdi], rdi");
		EMIT("call " << foreignName("_decref"));

		EMIT("pop rax");
	}

	getAddress(node->target.get(), "rbx");
	EMIT("mov qword [rbx], rax");
}

void CodeGen::visit(LetNode* node)
{
	node->value->accept(this);

	// We lose a reference to the original contents, and gain a reference to the
	// new rhs
	if (node->symbol->typeScheme->isBoxed())
	{
		EMIT("push rax");

		EMIT("mov rdi, rax");
		EMIT("call " << foreignName("_incref"));

		EMIT("mov rdi, " << access(*node->symbol));
		EMIT("call " << foreignName("_decref"));

		EMIT("pop rax");
	}

	EMIT("mov " << access(*node->symbol) << ", rax");
}

void CodeGen::visit(MatchNode* node)
{
	node->body->accept(this);
	EMIT("push rax");

	// Decrement references to the existing variables
	for (size_t i = 0; i < node->symbols.size(); ++i)
	{
		const Symbol& member = *node->symbols.at(i);

		if (member.typeScheme->isBoxed())
		{
			EMIT("mov rdi, " << access(member));
			EMIT("call " << foreignName("_decref"));
		}
	}

	EMIT("pop rsi");

	FunctionType* functionType = node->constructorSymbol->typeScheme->type()->get<FunctionType>();
	auto& constructor = functionType->output()->valueConstructors().front();

	// Copy over each of the members of the constructor pattern
	for (size_t i = 0; i < node->symbols.size(); ++i)
	{
		const Symbol& member = *node->symbols.at(i);
		size_t location = constructor->members().at(i).location;

		EMIT("mov rdi, [rsi + " << sizeof(SplObject) + 8 * location << "]");
		EMIT("mov " << access(member) << ", rdi");
	}

	// Increment references to the new variables
	for (size_t i = 0; i < constructor->boxedMembers(); ++i)
	{
		EMIT("mov rdi, [rsi + " << sizeof(SplObject) + 8 * i << "]");
		EMIT("call " << foreignName("_incref"));
	}
}

void CodeGen::visit(FunctionDefNode* node)
{
}

void CodeGen::visit(DataDeclaration* node)
{
	dataDeclarations_.push_back(node);
}

void CodeGen::visit(FunctionCallNode* node)
{
	for (auto i = node->arguments.rbegin(); i != node->arguments.rend(); ++i)
	{
		(*i)->accept(this);
		EMIT("push rax");
	}

	if (node->symbol->kind == kFunction && node->symbol->asFunction()->isBuiltin)
	{
		if (node->target == "not")
		{
			EMIT("pop rax");
			EMIT("xor rax, 10b");
		}
		else if (node->target == "head")
		{
			EMIT("pop rax");

			std::string good = uniqueLabel();

			EMIT("cmp rax, 0");
			EMIT("jne " << good);

			// If the list is null, then fail
		    EMIT("mov rbx, rsp");
		    EMIT("and rsp, -16");
		    EMIT("add rsp, -8");
		    EMIT("push rbx");
		    EMIT("mov rdi, 0");
		    EMIT("call _" << foreignName("die"));
		    EMIT("pop rbx");
		    EMIT("mov rsp, rbx");

			EMIT_LABEL(good);
			EMIT("mov rax, qword [rax + "<< offsetof(List, value) << "]");
		}
		else if (node->target == "tail")
		{
			EMIT("pop rax");

			std::string good = uniqueLabel();

			EMIT("cmp rax, 0");
			EMIT("jne " << good);

			// If the list is null, then fail
		    EMIT("mov rbx, rsp");
		    EMIT("and rsp, -16");
		    EMIT("add rsp, -8");
		    EMIT("push rbx");
		    EMIT("mov rdi, 1");
		    EMIT("call _" << foreignName("die"));
		    EMIT("pop rbx");
		    EMIT("mov rsp, rbx");

			EMIT_LABEL(good);
			EMIT("mov rax, qword [rax + " << offsetof(List, next) << "]");
		}
		else if (node->target == "Nil")
		{
			EMIT("mov rax, 0");
		}
		else if (node->target == "null")
		{
			std::string finish = uniqueLabel();
			EMIT("pop rax");
			EMIT("cmp rax, 0");
			EMIT("je " << finish);
			EMIT("mov rax, 11b");
			EMIT_LABEL(finish);
			EMIT("xor rax, 10b");
		}
		else if (node->target == "+")
		{
			EMIT("pop rax");
			EMIT("pop rbx");
			EMIT("xor rbx, 1");
			EMIT("add rax, rbx");
		}
		else if (node->target == "-")
		{
			EMIT("pop rax");
			EMIT("pop rbx");
			EMIT("xor rbx, 1");
			EMIT("sub rax, rbx");
		}
		else if (node->target == "*")
		{
			EMIT("pop rax");
			EMIT("pop rbx");
			EMIT("sar rax, 1");
			EMIT("sar rbx, 1");
			EMIT("imul rax, rbx");
			EMIT("lea rax, [2 * rax + 1]");
		}
		else if (node->target == "/")
		{
			EMIT("pop rax");
			EMIT("pop rbx");
			EMIT("sar rax, 1");
			EMIT("sar rbx, 1");
			EMIT("cqo");
			EMIT("idiv rbx");
			EMIT("lea rax, [2 * rax + 1]");
		}
		else if (node->target == "%")
		{
			EMIT("pop rax");
			EMIT("pop rbx");
			EMIT("sar rax, 1");
			EMIT("sar rbx, 1");
			EMIT("cqo");
			EMIT("idiv rbx");
			EMIT("mov rax, rdx");
			EMIT("lea rax, [2 * rax + 1]");
		}
		else
		{
			assert(false);
		}
	}
	else if (node->symbol->kind == kFunction && node->symbol->asFunction()->isForeign)
	{
		// x86_64 calling convention for C puts the first 6 arguments in registers
		if (node->arguments.size() >= 1) EMIT("pop rdi");
		if (node->arguments.size() >= 2) EMIT("pop rsi");
		if (node->arguments.size() >= 3) EMIT("pop rdx");
		if (node->arguments.size() >= 4) EMIT("pop rcx");
		if (node->arguments.size() >= 5) EMIT("pop r8");
		if (node->arguments.size() >= 6) EMIT("pop r9");

		// Realign the stack to 16 bytes (may not be necessary on all platforms)
	    EMIT("mov rbx, rsp");
	    EMIT("and rsp, -16");
	    EMIT("add rsp, -8");
	    EMIT("push rbx");

	    EMIT("call " << foreignName(mangle(node->target)));

	    // Undo the stack alignment
	    EMIT("pop rbx");
	    EMIT("mov rsp, rbx");
	}
	else if (node->symbol->kind == kFunction)
	{
		if (node->symbol->asFunction()->definition &&
			visitedFunctions_.find(node->symbol->asFunction()->definition) == visitedFunctions_.end())
		{
			referencedFunctions_.insert(node->symbol->asFunction()->definition);
		}

		EMIT("call _" << mangle(node->target));

		size_t args = node->arguments.size();
		if (args > 0) EMIT("add rsp, " << 8 * args);
	}
	else /* node->symbol->kind == kVariable */
	{
		// The variable represents a closure, so extract the actual function
		// address
		EMIT("mov rax, " << access(*node->symbol));
		EMIT("mov rax, qword [rax + " << sizeof(SplObject) << "]");
		EMIT("call rax");

		size_t args = node->arguments.size();
		if (args > 0) EMIT("add rsp, " << 8 * args);
	}
}

void CodeGen::visit(ReturnNode* node)
{
	node->expression->accept(this);

	EMIT("jmp __end_" << currentFunction_);
}

void CodeGen::visit(VariableNode* node)
{
	assert(node->symbol->kind == kVariable);

	EMIT("mov rax, " << access(*node->symbol));
}

//// Structures ////////////////////////////////////////////////////////////////

void CodeGen::visit(StructDefNode* node)
{
	structDeclarations_.push_back(node);
}


void CodeGen::visit(MemberAccessNode* node)
{
	EMIT("mov rax, " << access(*node->varSymbol));
	EMIT("mov rax, qword [rax + " << sizeof(SplObject) + 8 * node->memberLocation << "]");
}
