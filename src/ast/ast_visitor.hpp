#ifndef AST_VISITOR_HPP
#define AST_VISITOR_HPP

#include <vector>

class AssertNode;
class AssignNode;
class BinopNode;
class BlockNode;
class BoolNode;
class BreakNode;
class CastNode;
class ComparisonNode;
class ConstructorSpec;
class ContinueNode;
class DataDeclaration;
class ExternalFunctionCallNode;
class ForeignDeclNode;
class ForeverNode;
class ForNode;
class FunctionCallNode;
class FunctionDefNode;
class IfElseNode;
class IfNode;
class ImplNode;
class IndexNode;
class IntNode;
class LetNode;
class LogicalNode;
class MatchArm;
class MatchNode;
class MemberAccessNode;
class MemberDefNode;
class MethodCallNode;
class MethodDefNode;
class NullaryNode;
class PassNode;
class ProgramNode;
class ReturnNode;
class StringLiteralNode;
class StructDefNode;
class TraitDefNode;
class TraitMethodNode;
class TypeAliasNode;
class TypeName;
class VariableDefNode;
class WhileNode;

#define UNSUPPORTED(T) virtual void visit(T* node) { assert(false); }

class AstVisitor
{
public:
	// Default implementations do nothing but visit each child
	virtual void visit(AssertNode* node);
	virtual void visit(AssignNode* node);
	virtual void visit(BinopNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(CastNode* node);
	virtual void visit(ComparisonNode* node);
	virtual void visit(DataDeclaration* node);
	virtual void visit(ForNode* node);
	virtual void visit(ForeverNode* node);
	virtual void visit(FunctionCallNode* node);
	virtual void visit(FunctionDefNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(ImplNode* node);
	virtual void visit(IndexNode* node);
	virtual void visit(LetNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(MatchArm* node);
	virtual void visit(MatchNode* node);
	virtual void visit(MemberAccessNode* node);
	virtual void visit(MethodCallNode* node);
	virtual void visit(MethodDefNode* node);
	virtual void visit(ProgramNode* node);
	virtual void visit(ReturnNode* node);
	virtual void visit(StructDefNode* node);
	virtual void visit(TraitDefNode* node);
	virtual void visit(VariableDefNode* node);
	virtual void visit(WhileNode* node);

	// Leaf nodes
	virtual void visit(BoolNode* node) {}
	virtual void visit(BreakNode* node) {}
	virtual void visit(ConstructorSpec* node) {}
	virtual void visit(ContinueNode* node) {}
	virtual void visit(ForeignDeclNode* node) {}
	virtual void visit(IntNode* node) {}
	virtual void visit(MemberDefNode* node) {}
	virtual void visit(NullaryNode* node) {}
	virtual void visit(PassNode* node) {}
	virtual void visit(StringLiteralNode* node) {}
	virtual void visit(TraitMethodNode* node) {}
	virtual void visit(TypeAliasNode* node) {}
	virtual void visit(TypeName* node) {}
};

#endif
