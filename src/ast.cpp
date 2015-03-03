#include <cassert>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "ast.hpp"
#include "tokens.hpp"
#include "parser.hpp"

using namespace std;

AstNode::AstNode(const YYLTYPE& location)
: location(location)
{
}

AstNode::~AstNode()
{
}

std::string TypeName::str() const
{
    std::stringstream ss;

    ss << _name;
    for (auto& param : _parameters)
    {
        ss << " " << param->str();
    }

    return ss.str();
}

void ProgramNode::append(AstNode* child)
{
	children.emplace_back(child);
}


void BlockNode::append(StatementNode* child)
{
	children.emplace_back(child);
}

BlockNode* makeForNode(const YYLTYPE& location, const std::string& loopVar, ExpressionNode* list, StatementNode* body)
{
    // We need a unique variable name for the variable which holds the list
    // we are iterating over
    static int uniqueId = 0;

    std::string listVar = std::string("_for_list_") + boost::lexical_cast<std::string>(uniqueId++);

    ArgList headArgList;
    headArgList.emplace_back(new NullaryNode(location, listVar.c_str()));

    BlockNode* newBody = new BlockNode(location);
    newBody->append(
        new LetNode(location, loopVar, nullptr,
            new FunctionCallNode(location, "head", std::move(headArgList))));
    newBody->append(body);

    ArgList tailArgList;
    tailArgList.emplace_back(new NullaryNode(location, listVar.c_str()));
    newBody->append(
        new AssignNode(
            location,
            listVar,
            new FunctionCallNode(location, "tail", std::move(tailArgList))));

    BlockNode* forNode = new BlockNode(location);
    forNode->append(new LetNode(location, listVar.c_str(), nullptr, list));

    ArgList argList;
    argList.emplace_back(new NullaryNode(location, listVar.c_str()));

    ArgList argList2;
    argList2.emplace_back(new FunctionCallNode(location, "null", std::move(argList)));
    forNode->append(
        new WhileNode(location,
            new FunctionCallNode(location, "not", std::move(argList2)),
            newBody));

    return forNode;
}

FunctionCallNode* makeList(const YYLTYPE& location, ArgList& elements)
{
    FunctionCallNode* result = new FunctionCallNode(location, "Nil", ArgList());

    for (auto i = elements.rbegin(); i != elements.rend(); ++i)
    {
        ArgList argList;
        argList.emplace_back(std::move(*i));
        argList.emplace_back(result);

        result = new FunctionCallNode(location, "Cons", std::move(argList));
    }

    return result;
}

FunctionCallNode* makeString(const YYLTYPE& location, const std::string& s)
{
    FunctionCallNode* result = new FunctionCallNode(location, "Nil", ArgList());

    for (auto i = s.rbegin(); i != s.rend(); ++i)
    {
        ArgList argList;
        argList.emplace_back(new IntNode(location, *i));
        argList.emplace_back(result);

        result = new FunctionCallNode(location, "Cons", std::move(argList));
    }

    return result;
}

