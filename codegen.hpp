#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <vector>
#include "ast.hpp"
#include "ast_visitor.hpp"

class CodeGen : public AstVisitor
{
public:
	CodeGen() : labels_(0), out_(std::cout) {}

	virtual void visit(ProgramNode* node);
	virtual void visit(ComparisonNode* node);
	virtual void visit(BinaryOperatorNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(WhileNode* node);
	virtual void visit(AssignNode* node);
	virtual void visit(LetNode* node);
	virtual void visit(NullaryNode* node);
	virtual void visit(IntNode* node);
	virtual void visit(BoolNode* node);
	virtual void visit(NilNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(ReturnNode* node);
	virtual void visit(NullNode* node);

	// Generate a code fragment to access the symbol with the given name in the
	// current scope.
	std::string access(const VariableSymbol* symbol);

	// Gets the list of all external symbols which are referenced in this module
	std::vector<std::string> getExterns(ProgramNode* node);

	// Converts periods to underscores to make interfacing with C programs easier
	std::string mangle(const std::string& name);

private:
	std::string uniqueLabel() { return std::string("__") + boost::lexical_cast<std::string>(labels_++); }
	int labels_;
	std::ostream& out_;

	// The name of the function currently being generated
	std::string currentFunction_;

	// Keep track of the function definitions so that we can walk through them after the main function
	std::vector<FunctionDefNode*> functionDefs_;
};

#endif
