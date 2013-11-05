#ifndef AST_VISITOR_HPP
#define AST_VISITOR_HPP

#include <vector>
#include "scope.hpp"

class AssignNode;
class BinaryOperatorNode;
class BlockNode;
class BoolNode;
class ComparisonNode;
class DataDeclaration;
class ExternalFunctionCallNode;
class ForeignDeclNode;
class FunctionCallNode;
class FunctionDefNode;
class IfElseNode;
class IfNode;
class IntNode;
class LetNode;
class LogicalNode;
class NilNode;
class NullaryNode;
class NullNode;
class ParamListNode;
class ProgramNode;
class ReturnNode;
class WhileNode;

class AstVisitor
{
public:
	AstVisitor() : typeTable_(nullptr) {}

	// Default implementations do nothing but visit each child
	virtual void visit(AssignNode* node);
	virtual void visit(BinaryOperatorNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(ComparisonNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(LetNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(NullNode* node);
	virtual void visit(ProgramNode* node);
	virtual void visit(ReturnNode* node);
	virtual void visit(WhileNode* node);

	// Leaf nodes
	virtual void visit(BoolNode* node) {}
	virtual void visit(DataDeclaration* node) {}
	virtual void visit(ForeignDeclNode* node) {}
	virtual void visit(IntNode* node) {}
	virtual void visit(NilNode* node) {}
	virtual void visit(NullaryNode* node) {}
	virtual void visit(ParamListNode* node) {}

protected:
	Scope* topScope() { return scopes_.back(); }
	Symbol* searchScopes(const std::string& name);
	void enterScope(Scope* scope) { scopes_.push_back(scope); }
	void exitScope() { scopes_.pop_back(); }

	std::vector<Scope*> scopes_;

	TypeTable* typeTable_;
};

#endif
