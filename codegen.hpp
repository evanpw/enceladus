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

	virtual void visit(AssignNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(BoolNode* node);
	virtual void visit(DataDeclaration* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(IntNode* node);
	virtual void visit(LetNode* node);
	virtual void visit(MatchNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(NullaryNode* node);
	virtual void visit(ProgramNode* node);
	virtual void visit(ReturnNode* node);
	virtual void visit(WhileNode* node);

	// Generate a code fragment to access the symbol with the given name in the
	// current scope.
	std::string access(const VariableSymbol* symbol);

	// Gets the list of all external symbols which are referenced in this module
	std::vector<std::string> getExterns(ProgramNode* node);

	// Converts periods to underscores to make interfacing with C programs easier
	std::string mangle(const std::string& name);

private:
	// Helper functions that build commonly-occuring or complicated code
	void comparisonOperator(const std::string& op);
	void arithmeticOperator(const std::string& op);
	void decref(const VariableSymbol* symbol);
	void createConstructor(ValueConstructor* constructor);

	std::string foreignName(const std::string& name);

	std::string uniqueLabel() { return std::string("__") + boost::lexical_cast<std::string>(labels_++); }
	int labels_;
	std::ostream& out_;

	// The name of the function currently being generated
	std::string currentFunction_;

	// Keep track of the function & data type definitions so that we can walk
	// through them after the main function
	std::vector<FunctionDefNode*> functionDefs_;
	std::vector<DataDeclaration*> dataDeclarations_;
};

#endif
