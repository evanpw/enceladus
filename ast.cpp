#include <cassert>
#include <iostream>
#include "ast.hpp"
#include "simple.tab.h"

using namespace std;

AstNode::AstNode()
: type_(kNone)
{
	location_ = new YYLTYPE(yylloc);
}

AstNode::~AstNode()
{
	delete location_;
}

void ProgramNode::prepend(AstNode* child)
{
	children_.emplace_back(child);
}

void ParamListNode::prepend(const char* param)
{
    names_.emplace_back(param);
}

void BlockNode::prepend(StatementNode* child)
{
	children_.emplace_back(child);
}
