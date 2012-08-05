#ifndef AST_VISITOR_HPP
#define AST_VISITOR_HPP

class ProgramNode;
class NotNode;
class ComparisonNode;
class BinaryOperatorNode;
class LogicalNode;
class BlockNode;
class IfNode;
class IfElseNode;
class PrintNode;
class ReadNode;
class AssignNode;
class LabelNode;
class VariableNode;
class IntNode;
class GotoNode;

class AstVisitor
{
public:
	// Default implementations do nothing but visit each child
	virtual void visit(ProgramNode* node);
	virtual void visit(NotNode* node);
	virtual void visit(ComparisonNode* node);
	virtual void visit(BinaryOperatorNode* node);
	virtual void visit(LogicalNode* node);
	virtual void visit(BlockNode* node);
	virtual void visit(IfNode* node);
	virtual void visit(IfElseNode* node);
	virtual void visit(PrintNode* node);
	virtual void visit(ReadNode* node);
	virtual void visit(AssignNode* node);
	
	// Leaf nodes
	virtual void visit(LabelNode* node) {}
	virtual void visit(VariableNode* node) {}
	virtual void visit(IntNode* node) {}
	virtual void visit(GotoNode* node) {}
};

#endif
