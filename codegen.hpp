#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include <boost/lexical_cast.hpp>
#include <iostream>
#include "ast.hpp"
#include "ast_visitor.hpp"

class CodeGen : public AstVisitor
{
public:
	CodeGen() : labels_(0), out_(std::cout) {}
	
	virtual void visit(ProgramNode* node);
	virtual void visit(NotNode* node);
	virtual void visit(ComparisonNode* node);
	virtual void visit(BinaryOperatorNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(PrintNode* node);
	virtual void visit(ReadNode* node);
	virtual void visit(AssignNode* node);
	virtual void visit(LabelNode* node);
	virtual void visit(VariableNode* node);
	virtual void visit(IntNode* node);
	virtual void visit(GotoNode* node);
	
private:	
	std::string uniqueLabel() { return std::string("__") + boost::lexical_cast<std::string>(labels_++); }  
	int labels_;
	std::ostream& out_;
};

#endif
