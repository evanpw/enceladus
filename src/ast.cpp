#include <cassert>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "ast.hpp"
#include "ast_context.hpp"
#include "tokens.hpp"
#include "parser.hpp"

using namespace std;

AstNode::AstNode(AstContext* context, const YYLTYPE& location)
: location(location)
{
    context->addToContext(this);
}

AstNode::~AstNode()
{
}

std::string TypeName::str() const
{
    std::stringstream ss;

    ss << name;
    for (auto& param : parameters)
    {
        ss << " " << param->str();
    }

    return ss.str();
}

BlockNode* makeForNode(AstContext* context, const YYLTYPE& location, const std::string& loopVar, ExpressionNode* list, StatementNode* body)
{
    // We need a unique variable name for the variable which holds the list
    // we are iterating over
    static int uniqueId = 0;
    std::string listVar = std::string("_for_list_") + boost::lexical_cast<std::string>(uniqueId++);

    std::vector<ExpressionNode*> headArgList;
    headArgList.push_back(new NullaryNode(context, location, listVar.c_str()));

    BlockNode* newBody = new BlockNode(context, location);
    newBody->children.push_back(
        new LetNode(context, location, loopVar, nullptr,
            new FunctionCallNode(context, location, "head", std::move(headArgList))));
    newBody->children.push_back(body);

    std::vector<ExpressionNode*> tailArgList;
    tailArgList.push_back(new NullaryNode(context, location, listVar.c_str()));
    newBody->children.push_back(
        new AssignNode(
            context,
            location,
            listVar,
            new FunctionCallNode(context, location, "tail", std::move(tailArgList))));

    BlockNode* forNode = new BlockNode(context, location);
    forNode->children.push_back(new LetNode(context, location, listVar.c_str(), nullptr, list));

    std::vector<ExpressionNode*> argList;
    argList.push_back(new NullaryNode(context, location, listVar.c_str()));

    std::vector<ExpressionNode*> argList2;
    argList2.push_back(new FunctionCallNode(context, location, "null", std::move(argList)));
    forNode->children.push_back(
        new WhileNode(context, location,
            new FunctionCallNode(context, location, "not", std::move(argList2)),
            newBody));

    return forNode;
}

FunctionCallNode* makeList(AstContext* context, const YYLTYPE& location, std::vector<ExpressionNode*>& elements)
{
    FunctionCallNode* result = new FunctionCallNode(context, location, "Nil", {});

    for (auto i = elements.rbegin(); i != elements.rend(); ++i)
    {
        std::vector<ExpressionNode*> argList = {*i, result};
        result = new FunctionCallNode(context, location, "Cons", std::move(argList));
    }

    return result;
}

int StringLiteralNode::nextCounter = 0;

