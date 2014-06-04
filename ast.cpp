#include <cassert>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "ast.hpp"
#include "parser.hpp"

using namespace std;

AstNode::AstNode()
: location(new YYLTYPE(yylloc))
{
}

AstNode::~AstNode()
{
}

void ProgramNode::append(AstNode* child)
{
	children.emplace_back(child);
}


void BlockNode::append(StatementNode* child)
{
	children.emplace_back(child);
}

BlockNode* makeForNode(const std::string& loopVar, ExpressionNode* list, StatementNode* body)
{
    // We need a unique variable name for the variable which holds the list
    // we are iterating over
    static int uniqueId = 0;

    std::string listVar = std::string("_for_list_") + boost::lexical_cast<std::string>(uniqueId++);

    ArgList headArgList;
    headArgList.emplace_back(new NullaryNode(listVar.c_str()));

    BlockNode* newBody = new BlockNode;
    newBody->append(
        new LetNode(loopVar, nullptr,
            new FunctionCallNode("head", std::move(headArgList))));
    newBody->append(body);

    ArgList tailArgList;
    tailArgList.emplace_back(new NullaryNode(listVar.c_str()));
    newBody->append(
        new AssignNode(
            new VariableNode(listVar),
            new FunctionCallNode("tail", std::move(tailArgList))));

    BlockNode* forNode = new BlockNode;
    forNode->append(new LetNode(listVar.c_str(), nullptr, list));

    ArgList argList;
    argList.emplace_back(new NullaryNode(listVar.c_str()));

    ArgList argList2;
    argList2.emplace_back(new FunctionCallNode("null", std::move(argList)));
    forNode->append(
        new WhileNode(
            new FunctionCallNode("not", std::move(argList2)),
            newBody));

    return forNode;
}

FunctionCallNode* makeList(ArgList& elements)
{
    FunctionCallNode* result = new FunctionCallNode("Nil", ArgList());

    for (auto i = elements.rbegin(); i != elements.rend(); ++i)
    {
        ArgList argList;
        argList.emplace_back(std::move(*i));
        argList.emplace_back(result);

        result = new FunctionCallNode("Cons", std::move(argList));
    }

    return result;
}

FunctionCallNode* makeString(const std::string& s)
{
    FunctionCallNode* result = new FunctionCallNode("Nil", ArgList());

    for (auto i = s.rbegin(); i != s.rend(); ++i)
    {
        ArgList argList;
        argList.emplace_back(new IntNode(*i));
        argList.emplace_back(result);

        result = new FunctionCallNode("Cons", std::move(argList));
    }

    return result;
}

