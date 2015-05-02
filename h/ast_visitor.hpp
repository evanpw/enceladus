#ifndef AST_VISITOR_HPP
#define AST_VISITOR_HPP

#include <vector>
#include "scope.hpp"

class AssignNode;
class BlockNode;
class BoolNode;
class BreakNode;
class ComparisonNode;
class DataDeclaration;
class ExternalFunctionCallNode;
class ForeignDeclNode;
class ForeverNode;
class FunctionCallNode;
class FunctionDefNode;
class IfElseNode;
class IfNode;
class IntNode;
class LetNode;
class LogicalNode;
class MatchArm;
class MatchNode;
class MemberAccessNode;
class MemberDefNode;
class NullaryNode;
class ProgramNode;
class ReturnNode;
class StructDefNode;
class SwitchNode;
class TypeAliasNode;
class VariableNode;
class WhileNode;

class AstVisitor
{
public:
	// Default implementations do nothing but visit each child
	virtual void visit(AssignNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(ComparisonNode* node);
	virtual void visit(ForeverNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(LetNode* node);
	virtual void visit(MatchArm* node);
	virtual void visit(MatchNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(ProgramNode* node);
	virtual void visit(ReturnNode* node);
	virtual void visit(StructDefNode* node);
	virtual void visit(SwitchNode* node);
	virtual void visit(WhileNode* node);

	// Leaf nodes
	virtual void visit(BoolNode* node) {}
	virtual void visit(BreakNode* node) {}
	virtual void visit(DataDeclaration* node) {}
	virtual void visit(ForeignDeclNode* node) {}
	virtual void visit(IntNode* node) {}
	virtual void visit(MemberAccessNode* node) {}
	virtual void visit(MemberDefNode* node) {}
	virtual void visit(NullaryNode* node) {}
	virtual void visit(TypeAliasNode* node) {}
	virtual void visit(VariableNode* node) {}
};

#endif
