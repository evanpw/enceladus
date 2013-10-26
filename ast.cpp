#include <cassert>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "ast.hpp"
#include "simple.tab.h"

using namespace std;

AstNode::AstNode()
: type_(&Type::Void)
{
	location_ = new YYLTYPE(yylloc);
}

AstNode::~AstNode()
{
	delete location_;
}

void ProgramNode::append(AstNode* child)
{
	children_.emplace_back(child);
}

void ParamListNode::append(const char* param)
{
    names_.emplace_back(param);
}

void BlockNode::append(StatementNode* child)
{
	children_.emplace_back(child);
}

BlockNode* makeForNode(const char* loopVar, ExpressionNode* list, StatementNode* body)
{
    // We need a unique variable name for the variable which holds the list
    // we are iterating over
    static int uniqueId = 0;

    std::string listVarStr = std::string("_for_list_") + boost::lexical_cast<std::string>(uniqueId++);
    const char* listVar = StringTable::add(listVarStr.c_str());

    BlockNode* newBody = new BlockNode;
    newBody->append(
        new LetNode(loopVar, "Int",          // FIXME: Lists of other types
            new HeadNode(
                new VariableNode(listVar))));
    newBody->append(body);
    newBody->append(
        new AssignNode(listVar,
            new TailNode(
                new VariableNode(listVar))));

    BlockNode* forNode = new BlockNode;
    forNode->append(new LetNode(listVar, "List", list));
    forNode->append(
        new WhileNode(
            new NotNode(
                new NullNode(
                    new VariableNode(listVar))),
        newBody));

    return forNode;
}