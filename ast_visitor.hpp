#ifndef AST_VISITOR_HPP
#define AST_VISITOR_HPP

#include <vector>
#include "scope.hpp"

class ProgramNode;
class ComparisonNode;
class BinaryOperatorNode;
class LogicalNode;
class BlockNode;
class IfNode;
class IfElseNode;
class WhileNode;
class AssignNode;
class NullaryNode;
class IntNode;
class BoolNode;
class FunctionDefNode;
class ForeignDeclNode;
class FunctionCallNode;
class ExternalFunctionCallNode;
class ReturnNode;
class ParamListNode;
class NilNode;
class LetNode;
class HeadNode;
class TailNode;
class NullNode;

class AstVisitor
{
public:
	// Default implementations do nothing but visit each child
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
	virtual void visit(FunctionDefNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(ReturnNode* node);
	virtual void visit(HeadNode* node);
	virtual void visit(TailNode* node);
	virtual void visit(NullNode* node);

	// Leaf nodes
	virtual void visit(NullaryNode* node) {}
	virtual void visit(IntNode* node) {}
	virtual void visit(BoolNode* node) {}
	virtual void visit(ParamListNode* node) {}
	virtual void visit(NilNode* node) {}
	virtual void visit(ForeignDeclNode* node) {}

protected:
	Scope* topScope() { return scopes_.back(); }
	Symbol* searchScopes(const std::string& name);
	void enterScope(Scope* scope) { scopes_.push_back(scope); }
	void exitScope() { scopes_.pop_back(); }

	std::vector<Scope*> scopes_;
};

#endif
