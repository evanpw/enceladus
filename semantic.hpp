#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast.hpp"

class SemanticPass1 : public AstVisitor
{
public:
	virtual void visit(LabelNode* node);
	virtual void visit(VariableNode* node);
};

class SemanticPass2 : public AstVisitor
{
public:
	virtual void visit(GotoNode* node);
};

#endif
