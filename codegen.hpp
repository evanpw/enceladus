#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <set>
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
	virtual void visit(ComparisonNode* node);
	virtual void visit(DataDeclaration* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(IntNode* node);
	virtual void visit(LetNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(MatchNode* node);
	virtual void visit(MemberAccessNode* node);
	virtual void visit(MemberDefNode* node);
	virtual void visit(NullaryNode* node);
	virtual void visit(ProgramNode* node);
	virtual void visit(ReturnNode* node);
	virtual void visit(StringNode* node);
	virtual void visit(StructDefNode* node);
	virtual void visit(StructInitNode* node);
	virtual void visit(VariableNode* node);
	virtual void visit(WhileNode* node);

	// Generate a code fragment to access the symbol with the given name in the
	// current scope.
	std::string access(const VariableSymbol* symbol);

	// Emit code to move the address of the given assignable location to the
	// given register
	void getAddress(AssignableNode* node, const std::string& dest);

	// Gets the list of all external symbols which are referenced in this module
	std::vector<std::string> getExterns(ProgramNode* node);

	// Converts periods to underscores to make interfacing with C programs easier
	std::string mangle(const std::string& name);

	// Create the functions corresponding to a data type declaration
	void createConstructor(ValueConstructor* constructor);
	void createStructInit(StructDefNode* node);

private:
	void decref(const VariableSymbol* symbol);

	std::string foreignName(const std::string& name);

	std::string uniqueLabel() { return std::string("__") + boost::lexical_cast<std::string>(labels_++); }
	int labels_;
	std::ostream& out_;

	// The name of the function currently being generated
	std::string currentFunction_;

	// Keep track of referenced and visited functions so that we generate code
	// only for those functions referenced directly or indirectly by the main
	// function
	std::set<FunctionDefNode*> visitedFunctions_;
	std::set<FunctionDefNode*> referencedFunctions_;

	// Keep track of the function & data type definitions so that we can walk
	// through them after the main function
	std::vector<DataDeclaration*> dataDeclarations_;
	std::vector<StructDefNode*> structDeclarations_;

	// All string literals
	std::vector<StringNode*> stringLiterals_;
};

#endif
