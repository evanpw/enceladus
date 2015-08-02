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

