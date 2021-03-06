#include "ast/ast.hpp"
#include "ast/ast_context.hpp"
#include "parser/parser.hpp"
#include "parser/tokens.hpp"

#include <boost/lexical_cast.hpp>
#include <cassert>
#include <iostream>

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

    if (!parameters.empty())
    {
        ss << "<";

        bool first = true;
        for (auto& param : parameters)
        {
            if (!first) ss << ", ";
            ss << param->str();
            first = false;
        }

        ss << ">";
    }

    return ss.str();
}

FunctionCallNode* createList(AstContext* context, const YYLTYPE& location, std::vector<ExpressionNode*>& elements)
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
int LambdaNode::nextCounter = 0;
