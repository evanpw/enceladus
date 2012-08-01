#include <iostream>
#include <map>
#include "codegen.hpp"
#include "symbol_table.hpp"

void CodeGen::visit(ProgramNode* node)
{
	out_ << "section .text" << std::endl;
	out_ << "global __main" << std::endl;
	out_ << "extern __read, __print, __exit" << std::endl; 
	out_ << "__main:" << std::endl;
	
	for (std::list<AstNode*>::const_iterator i = node->children().begin(); i != node->children().end(); ++i)
	{
		(*i)->accept(this);	
	}
	
	out_ << "call __exit" << std::endl;
	
	out_ << "section .data" << std::endl;
	const std::map<const char*, Symbol*>& symbols = SymbolTable::symbols();
	std::map<const char*, Symbol*>::const_iterator i;
	for (i = symbols.begin(); i != symbols.end(); ++i)
	{
		if (i->second->type == kVariable)
		{
			out_ << "_" << i->second->name << ": dd 0" << std::endl;
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
	
	std::string trueBranch = uniqueLabel();
	std::string endLabel = uniqueLabel();
	
	out_ << "cmp eax, 0" << std::endl;
	out_ << "je " << trueBranch << std::endl;
	out_ << "mov eax, 0" << std::endl;
	out_ << "jmp " << endLabel << std::endl;
	out_ << trueBranch << ":" << std::endl;
	out_ << "mov eax, 1" << std::endl;
	out_ << endLabel << ":" << std::endl;
}

void CodeGen::visit(GreaterNode* node)
{
	node->lhs()->accept(this);
	out_ << "push eax" << std::endl;
	node->rhs()->accept(this);
	out_ << "cmp dword [esp], eax" << std::endl;
	
	std::string trueBranch = uniqueLabel();
	std::string endLabel = uniqueLabel();
	
	out_ << "jg " << trueBranch << std::endl;
	out_ << "mov eax, 0" << std::endl;
	out_ << "jmp " << endLabel << std::endl;
	out_ << trueBranch << ":" << std::endl;
	out_ << "mov eax, 1" << std::endl;
	out_ << endLabel << ":" << std::endl;
	out_ << "pop ebx" << std::endl;
}

void CodeGen::visit(EqualNode* node)
{
	node->lhs()->accept(this);
	out_ << "push eax" << std::endl;
	node->rhs()->accept(this);
	out_ << "cmp dword [esp], eax" << std::endl;
	
	std::string trueBranch = uniqueLabel();
	std::string endLabel = uniqueLabel();
	
	out_ << "je " << trueBranch << std::endl;
	out_ << "mov eax, 0" << std::endl;
	out_ << "jmp " << endLabel << std::endl;
	out_ << trueBranch << ":" << std::endl;
	out_ << "mov eax, 1" << std::endl;
	out_ << endLabel << ":" << std::endl;
	out_ << "pop ebx" << std::endl;
}

void CodeGen::visit(PlusNode* node)
{
	node->lhs()->accept(this);
	out_ << "push eax" << std::endl;
	node->rhs()->accept(this);
	out_ << "add eax, dword [esp]" << std::endl;
	out_ << "pop ebx" << std::endl;
}

void CodeGen::visit(MinusNode* node)
{
	node->lhs()->accept(this);
	out_ << "push eax" << std::endl;
	node->rhs()->accept(this);
	out_ << "xchg eax, dword [esp]" << std::endl;
	out_ << "sub eax, dword [esp]" << std::endl;
	out_ << "pop ebx" << std::endl;
}

void CodeGen::visit(TimesNode* node)
{
	node->lhs()->accept(this);
	out_ << "push eax" << std::endl;
	node->rhs()->accept(this);
	out_ << "imul eax, dword [esp]" << std::endl;
	out_ << "pop ebx" << std::endl;
}

void CodeGen::visit(DivideNode* node)
{
	node->lhs()->accept(this);
	out_ << "push eax" << std::endl;
	node->rhs()->accept(this);
	out_ << "xchg eax, dword [esp]" << std::endl;
	out_ << "cdq" << std::endl;
	out_ << "idiv dword [esp]" << std::endl;
	out_ << "pop ebx" << std::endl;
}

void CodeGen::visit(VariableNode* node)
{
	out_ << "mov eax, dword [_" << node->name() << "]" << std::endl;
}

void CodeGen::visit(IntNode* node)
{
	out_ << "mov eax, " << node->value() << std::endl;
}

void CodeGen::visit(IfNode* node)
{
	node->condition()->accept(this);
	
	std::string endLabel = uniqueLabel();
	
	out_ << "cmp eax, 0" << std::endl;
	out_ << "je " << endLabel << std::endl;
	node->body()->accept(this);
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
	out_ << "mov dword [_" << node->target()->name() << "], eax" << std::endl;
}

void CodeGen::visit(AssignNode* node)
{
	node->value()->accept(this);
	out_ << "mov dword [_" << node->target()->name() << "], eax" << std::endl;
}