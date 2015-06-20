#include "parser.hpp"
#include "exceptions.hpp"
#include "tokens.hpp"

#include <iostream>
#include <boost/lexical_cast.hpp>

//// Lexing machinery //////////////////////////////////////////////////////////

// Two tokens of look-ahead
Token nextTokens[2];

// Calls the lexer
Token yylex();

void advance()
{
    nextTokens[0] = nextTokens[1];

    if (nextTokens[0].type != tEOF)
    {
        nextTokens[1] = yylex();
    }
}

void initialize()
{
    advance();
    advance();
}

//// Parsing machinery /////////////////////////////////////////////////////////

YYLTYPE getLocation()
{
    return nextTokens[0].location;
}

bool accept(TokenType t)
{
    if (nextTokens[0].type == t)
    {
        advance();
        return true;
    }

    return false;
}

bool accept(char c) { return accept((TokenType)c); }

Token expect(TokenType t)
{
    if (nextTokens[0].type == t)
    {
        Token token = nextTokens[0];
        advance();
        return token;
    }

    YYLTYPE location = getLocation();

    std::stringstream ss;

    ss << "Near line " << location.first_line << ", "
       << "column " << location.first_column << ": "
       << "expected " << tokenToString(t) << ", but got "
       << tokenToString(nextTokens[0].type);

    throw LexerError(ss.str());
}

Token expect(char c) { return expect((TokenType)c); }

TokenType peekType()
{
    return nextTokens[0].type;
}

TokenType peek2ndType()
{
    return nextTokens[1].type;
}

void Parser::parse()
{
    initialize();
    program();
}

////////////////////////////////////////////////////////////////////////////////
//// Grammar ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//// Statements ////////////////////////////////////////////////////////////////

ProgramNode* Parser::program()
{
    ProgramNode* node = new ProgramNode(_context, getLocation());

    while (!accept(tEOF))
    {
        node->children.push_back(statement());
    }

    _context.setRoot(node);

    return node;
}

StatementNode* Parser::statement()
{
    switch(peekType())
    {
    case tIF:
        return if_statement();

    case tDATA:
        return data_declaration();

    case tTYPE:
        return type_alias_declaration();

    case tDEF:
        return function_definition();

    case tFOR:
        return for_statement();

    case tFOREIGN:
        return foreign_declaration();

    case tFOREVER:
        return forever_statement();

    case tLET:
        return let_statement();

    case tMATCH:
        return match_statement();

    case tRETURN:
        return return_statement();

    case tSTRUCT:
        return struct_declaration();

    case tWHILE:
        return while_statement();

    case tVAR:
        return variable_declaration();

    case tBREAK:
        return break_statement();

    case tLIDENT:
        if (peek2ndType() == '{' ||
            peek2ndType() == '=' ||
            peek2ndType() == tPLUS_EQUAL ||
            peek2ndType() == tMINUS_EQUAL ||
            peek2ndType() == tTIMES_EQUAL ||
            peek2ndType() == tDIV_EQUAL)
        {
            return assignment_statement();
        }
        else if (peek2ndType() == tCOLON_EQUAL || peek2ndType() == ':')
        {
            return variable_declaration();
        }

        // Else fallthrough

    default:
        ExpressionNode* expressionNode = expression();
        expect(tEOL);
        return expressionNode;
    }
}

// Like an if statement, but doesn't match IF first
StatementNode* Parser::if_helper(const YYLTYPE& location)
{
    ExpressionNode* condition = expression();
    StatementNode* ifBody = suite();

    YYLTYPE intermediateLocation = getLocation();
    if (accept(tELIF))
    {
        StatementNode* elseBody = if_helper(intermediateLocation);
        return new IfElseNode(_context, location, condition, ifBody, elseBody);
    }
    else if (accept(tELSE))
    {
        StatementNode* elseBody = suite();
        return new IfElseNode(_context, location, condition, ifBody, elseBody);
    }
    else
    {
        return new IfNode(_context, location, condition, ifBody);
    }
}

StatementNode* Parser::if_statement()
{
    YYLTYPE location = getLocation();

    expect(tIF);
    return if_helper(location);
}

StatementNode* Parser::data_declaration()
{
    YYLTYPE location = getLocation();

    expect(tDATA);
    Token name = expect(tUIDENT);

    std::vector<std::string> typeParameters;
    while (peekType() == tLIDENT)
    {
        Token token = expect(tLIDENT);
        typeParameters.push_back(token.value.str);
    }

    expect('=');

    std::vector<ConstructorSpec*> specs;
    specs.push_back(constructor_spec());
    while (accept('|'))
    {
        specs.push_back(constructor_spec());
    }

    expect(tEOL);

    return new DataDeclaration(_context, location, name.value.str, typeParameters, specs);
}

StatementNode* Parser::type_alias_declaration()
{
    YYLTYPE location = getLocation();

    expect(tTYPE);
    Token name = expect(tUIDENT);
    expect('=');
    TypeName* typeName = type();
    expect(tEOL);

    return new TypeAliasNode(_context, location, name.value.str, typeName);
}

StatementNode* Parser::function_definition()
{
    YYLTYPE location = getLocation();

    expect(tDEF);
    std::string name = ident();
    std::pair<std::vector<std::string>, TypeName*> paramsAndTypes = params_and_types();

    StatementNode* body = suite();
    return new FunctionDefNode(_context, location, name, body, paramsAndTypes.first, paramsAndTypes.second);
}

StatementNode* Parser::foreign_declaration()
{
    YYLTYPE location = getLocation();

    expect(tFOREIGN);
    std::string name = ident();
    std::pair<std::vector<std::string>, TypeName*> paramsAndTypes = params_and_types();
    expect(tEOL);

    return new ForeignDeclNode(_context, location, name, paramsAndTypes.first, paramsAndTypes.second);
}

StatementNode* Parser::for_statement()
{
    YYLTYPE location = getLocation();

    expect(tFOR);
    Token loopVar = expect(tLIDENT);
    expect(tIN);
    ExpressionNode* listExpression = expression();
    StatementNode* body = suite();

    return makeForNode(_context, location, loopVar.value.str, listExpression, body);
}

StatementNode* Parser::forever_statement()
{
    YYLTYPE location = getLocation();

    expect(tFOREVER);
    StatementNode* body = suite();

    return new ForeverNode(_context, location, body);
}

StatementNode* Parser::let_statement()
{
    YYLTYPE location = getLocation();

    expect(tLET);
    Token constructor = expect(tUIDENT);
    std::vector<std::string> params = parameters();
    expect('=');
    ExpressionNode* body = expression();
    expect(tEOL);

    return new MatchNode(_context, location, constructor.value.str, params, body);
}

/// match_statement: MATCH expression EOL match_body
/// match_body: INDENT match_arm { match_arm } DEDENT
StatementNode* Parser::match_statement()
{
    YYLTYPE location = getLocation();

    expect(tMATCH);
    ExpressionNode* expr = expression();
    expect(tEOL);
    expect(tINDENT);

    std::vector<MatchArm*> arms;
    while (!accept(tDEDENT))
    {
        arms.push_back(match_arm());
    }

    return new SwitchNode(_context, location, expr, std::move(arms));
}

MatchArm* Parser::match_arm()
{
    YYLTYPE location = getLocation();
    Token constructor = expect(tUIDENT);
    std::vector<std::string> params = parameters();
    expect(tDARROW);

    if (accept(tEOL))
    {
        expect(tINDENT);

        BlockNode* block = new BlockNode(_context, getLocation());
        while (peekType() != tDEDENT)
        {
            block->children.push_back(statement());
        }

        expect(tDEDENT);

        return new MatchArm(_context, location, constructor.value.str, params, block);
    }
    else
    {
        StatementNode* body = statement();
        return new MatchArm(_context, location, constructor.value.str, params, body);
    }
}

StatementNode* Parser::return_statement()
{
    YYLTYPE location = getLocation();

    expect(tRETURN);
    ExpressionNode* value = expression();
    expect(tEOL);

    return new ReturnNode(_context, location, value);
}

StatementNode* Parser::struct_declaration()
{
    YYLTYPE location = getLocation();

    expect(tSTRUCT);
    Token name = expect(tUIDENT);
    expect('=');
    std::vector<MemberDefNode*> memberList = members();

    return new StructDefNode(_context, location, name.value.str, memberList);
}

StatementNode* Parser::while_statement()
{
    YYLTYPE location = getLocation();

    expect(tWHILE);
    ExpressionNode* condition = expression();
    StatementNode* body = suite();

    return new WhileNode(_context, location, condition, body);
}

StatementNode* Parser::assignment_statement()
{
    YYLTYPE location = getLocation();

    Token token = expect(tLIDENT);
    std::string lhs = token.value.str;

    if (accept('='))
    {
        ExpressionNode* rhs = expression();

        expect(tEOL);

        return new AssignNode(_context, location, lhs, rhs);
    }

    std::vector<ExpressionNode*> argList;
    argList.push_back(new VariableNode(_context, location, lhs));

    std::string functionName;
    switch (peekType())
    {
    case tPLUS_EQUAL:
        expect(tPLUS_EQUAL);
        functionName = '+';
        break;

    case tMINUS_EQUAL:
        expect(tMINUS_EQUAL);
        functionName = '-';
        break;

    case tTIMES_EQUAL:
        expect(tTIMES_EQUAL);
        functionName = '*';
        break;

    case tDIV_EQUAL:
        expect(tDIV_EQUAL);
        functionName = '/';
        break;

    default:
        std::cerr << "ERROR: Unexpected " << tokenToString(nextTokens[0].type) << std::endl;
        exit(1);

    }

    ExpressionNode* rhs = expression();
    argList.push_back(rhs);

    expect(tEOL);

    return new AssignNode(_context, location, lhs, new FunctionCallNode(_context, location, functionName, std::move(argList)));
}

StatementNode* Parser::variable_declaration()
{
    YYLTYPE location = getLocation();

    if (peekType() == tLIDENT)
    {
        Token varName = expect(tLIDENT);
        TypeName* varType = accept(':') ? type() : nullptr;
        expect(tCOLON_EQUAL);
        ExpressionNode* value = expression();
        expect(tEOL);

        return new LetNode(_context, location, varName.value.str, varType, value);
    }
    else
    {
        expect(tVAR);
        Token varName = expect(tLIDENT);
        TypeName* varType = accept(':') ? type() : nullptr;
        expect('=');
        ExpressionNode* value = expression();
        expect(tEOL);

        return new LetNode(_context, location, varName.value.str, varType, value);
    }
}

StatementNode* Parser::break_statement()
{
    YYLTYPE location = getLocation();

    expect(tBREAK);
    expect(tEOL);

    return new BreakNode(_context, location);
}

//// Miscellaneous /////////////////////////////////////////////////////////////

StatementNode* Parser::suite()
{
    if (accept(tEOL))
    {
        expect(tINDENT);

        BlockNode* block = new BlockNode(_context, getLocation());
        while (peekType() != tDEDENT)
        {
            block->children.push_back(statement());
        }

        expect(tDEDENT);

        return block;
    }
    else
    {
        expect(':');
        return statement();
    }
}

std::vector<std::string> Parser::parameters()
{
    std::vector<std::string> result;
    while (peekType() == tLIDENT)
    {
        Token param = expect(tLIDENT);

        result.push_back(param.value.str);
    }

    return result;
}

std::string Parser::ident()
{
    Token name;
    if (peekType() == tLIDENT)
    {
        name = expect(tLIDENT);
    }
    else if (peekType() == tUIDENT)
    {
        name = expect(tUIDENT);
    }
    else
    {
        std::cerr << "ERROR: Expected identifier, but got " << tokenToString(nextTokens[0].type) << std::endl;
        exit(1);
    }

    return name.value.str;
}

//// Types /////////////////////////////////////////////////////////////////////

/// type
///     : '|' [ arrow_type { ',' arrow_type } ] '|' RARROW constructed_type
///     | arrow_type
TypeName* Parser::type()
{
    YYLTYPE location = getLocation();

    if (!accept('|')) return arrow_type();

    TypeName* typeName = new TypeName(_context, location, "Function");

    if (peekType() != '|')
    {
        typeName->parameters.push_back(arrow_type());

        while (accept(','))
        {
            typeName->parameters.push_back(arrow_type());
        }
    }

    expect('|');
    expect(tRARROW);

    // Return type
    typeName->parameters.push_back(constructed_type());

    return typeName;
}

/// arrow_type
///     : constructed_type [ RARROW constructed_type ]
TypeName* Parser::arrow_type()
{
    YYLTYPE location = getLocation();

    TypeName* firstType = constructed_type();
    if (accept(tRARROW))
    {
        TypeName* functionType = new TypeName(_context, location, "Function");
        functionType->parameters.push_back(firstType);
        functionType->parameters.push_back(constructed_type());

        return functionType;
    }
    else
    {
        return firstType;
    }
}

/// constructed_type
///     : UIDENT { simple_type }
///     | simple_type
TypeName* Parser::constructed_type()
{
    YYLTYPE location = getLocation();

    if (peekType() == tUIDENT)
    {
        Token name = expect(tUIDENT);
        TypeName* typeName = new TypeName(_context, location, name.value.str);

        while (peekType() == tUIDENT || peekType() == tLIDENT || peekType() == '[' || peekType() == '(')
        {
            typeName->parameters.push_back(simple_type());
        }

        return typeName;
    }
    else
    {
        return simple_type();
    }
}

/// simple_type
///     : LIDENT
///     | UIDENT
///     | '[' type ']'
///     | '(' type ')'
TypeName* Parser::simple_type()
{
    YYLTYPE location = getLocation();

    if (peekType() == tUIDENT)
    {
        Token typeName = expect(tUIDENT);
        return new TypeName(_context, location, typeName.value.str);
    }
    else if (peekType() == tLIDENT)
    {
        Token typeName = expect(tLIDENT);
        return new TypeName(_context, location, typeName.value.str);
    }
    else if (peekType() == '(')
    {
        expect('(');
        TypeName* internalType = type();
        expect(')');

        return internalType;
    }
    else
    {
        expect('[');
        TypeName* internalType = type();
        expect(']');

        TypeName* typeName = new TypeName(_context, location, "List");
        typeName->parameters.push_back(internalType);

        return typeName;
    }
}

ConstructorSpec* Parser::constructor_spec()
{
    YYLTYPE location = getLocation();
    Token name = expect(tUIDENT);

    ConstructorSpec* constructorSpec = new ConstructorSpec(_context, location, name.value.str);
    while (peekType() == tUIDENT || peekType() == tLIDENT || peekType() == '[' || peekType() == '(')
    {
        constructorSpec->members.push_back(simple_type());
    }

    return constructorSpec;
}

std::pair<std::string, TypeName*> Parser::param_and_type()
{
    Token param = expect(tLIDENT);
    expect(':');
    TypeName* typeName = type();

    return {param.value.str, typeName};
}

/// type_declaration
///     : '(' [ LIDENT ':' type { ',' LIDENT ':' type } ] ')' RARROW constructed_type
std::pair<std::vector<std::string>, TypeName*> Parser::params_and_types()
{
    YYLTYPE location = getLocation();

    expect('(');

    std::vector<std::string> params;
    TypeName* typeName = new TypeName(_context, location, "Function");

    if (peekType() == tLIDENT)
    {
        std::pair<std::string, TypeName*> param_type = param_and_type();
        params.push_back(param_type.first);
        typeName->parameters.push_back(param_type.second);

        while (accept(','))
        {
            param_type = param_and_type();
            params.push_back(param_type.first);
            typeName->parameters.push_back(param_type.second);
        }
    }

    expect(')');
    expect(tRARROW);

    // Return type
    typeName->parameters.push_back(constructed_type());

    return {params, typeName};
}

 //// Structures ///////////////////////////////////////////////////////////////

std::vector<MemberDefNode*> Parser::members()
{
    std::vector<MemberDefNode*> memberList;

    if (accept(tEOL))
    {
        expect(tINDENT);

        while (peekType() != tDEDENT)
        {
            memberList.push_back(member_definition());
        }

        expect(tDEDENT);
    }
    else
    {
        memberList.push_back(member_definition());
    }

    return memberList;
}

MemberDefNode* Parser::member_definition()
{
    YYLTYPE location = getLocation();

    Token name = expect(tLIDENT);
    expect(':');
    TypeName* typeName = type();
    expect(tEOL);

    return new MemberDefNode(_context, location, name.value.str, typeName);
}

//// Expressions ///////////////////////////////////////////////////////////////

ExpressionNode* Parser::expression()
{
    ExpressionNode* lhs = and_expression();

    if (accept(tOR))
    {
        return new LogicalNode(_context, getLocation(), lhs, LogicalNode::kOr, expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::and_expression()
{
    ExpressionNode* lhs = equality_expression();

    if (accept(tAND))
    {
        return new LogicalNode(_context, getLocation(), lhs, LogicalNode::kAnd, and_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::equality_expression()
{
    ExpressionNode* lhs = relational_expression();

    if (accept(tEQUALS))
    {
        return new ComparisonNode(_context, getLocation(), lhs, ComparisonNode::kEqual, relational_expression());
    }
    else if (accept(tNE))
    {
        return new ComparisonNode(_context, getLocation(), lhs, ComparisonNode::kNotEqual, relational_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::relational_expression()
{
    ExpressionNode* lhs = cons_expression();

    if (accept('>'))
    {
        return new ComparisonNode(_context, getLocation(), lhs, ComparisonNode::kGreater, cons_expression());
    }
    else if (accept('<'))
    {
        return new ComparisonNode(_context, getLocation(), lhs, ComparisonNode::kLess, cons_expression());
    }
    else if (accept(tGE))
    {
        return new ComparisonNode(_context, getLocation(), lhs, ComparisonNode::kGreaterOrEqual, cons_expression());
    }
    else if (accept(tLE))
    {
        return new ComparisonNode(_context, getLocation(), lhs, ComparisonNode::kLessOrEqual, cons_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::cons_expression()
{
    ExpressionNode* lhs = additive_expression();

    if (accept(tDCOLON))
    {
        return new FunctionCallNode(_context, getLocation(), "Cons", {lhs, cons_expression()});
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::additive_expression()
{
    ExpressionNode* result = multiplicative_expression();

    while (peekType() == '+' || peekType() == '-')
    {
        if (accept('+'))
        {
            result = new FunctionCallNode(_context, getLocation(), "+", {result, multiplicative_expression()});
        }
        else
        {
            expect('-');
            result = new FunctionCallNode(_context, getLocation(), "-", {result, multiplicative_expression()});
        }
    }

    return result;
}

ExpressionNode* Parser::multiplicative_expression()
{
    ExpressionNode* result = concat_expression();

    while (peekType() == '*' || peekType() == '/' || peekType() == tMOD)
    {
        if (accept('*'))
        {
            result = new FunctionCallNode(_context, getLocation(), "*", {result, concat_expression()});
        }
        else if (accept('/'))
        {
            result = new FunctionCallNode(_context, getLocation(), "/", {result, concat_expression()});
        }
        else
        {
            expect(tMOD);
            result = new FunctionCallNode(_context, getLocation(), "%", {result, concat_expression()});
        }
    }

    return result;
}

ExpressionNode* Parser::concat_expression()
{
    ExpressionNode* lhs = negation_expression();

    if (accept(tCONCAT))
    {
        return new FunctionCallNode(_context, getLocation(), "concat", {lhs, concat_expression()});
    }
    else if (accept('^'))
    {
        return new FunctionCallNode(_context, getLocation(), "strCat", {lhs, concat_expression()});
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::negation_expression()
{
    if (accept('-'))
    {
        return new FunctionCallNode(_context, getLocation(), "-", {new IntNode(_context, getLocation(), 0), func_call_expression()});
    }
    else
    {
        return func_call_expression();
    }
}

static bool canStartUnaryExpression(TokenType t)
{
    return (t == '(' ||
            t == tTRUE ||
            t == tFALSE ||
            t == '[' ||
            t == tINT_LIT ||
            t == tSTRING_LIT ||
            t == tLIDENT ||
            t == tUIDENT);
}

ExpressionNode* Parser::func_call_expression()
{
    if ((peekType() == tLIDENT || peekType() == tUIDENT) && (canStartUnaryExpression(peek2ndType()) || peek2ndType() == '$'))
    {
        YYLTYPE location = getLocation();

        std::string functionName = ident();

        std::vector<ExpressionNode*> argList;
        while (canStartUnaryExpression(peekType()))
        {
            argList.push_back(unary_expression());
        }

        if (peekType() == '$')
        {
            expect('$');
            argList.push_back(expression());
        }

        return new FunctionCallNode(_context, location, functionName, std::move(argList));
    }
    else
    {
        return unary_expression();
    }
}

ExpressionNode* Parser::unary_expression()
{
    YYLTYPE location = getLocation();

    switch (peekType())
    {
    case '(':
    {
        expect('(');
        ExpressionNode* interior = expression();
        expect(')');

        return interior;
    }

    case tTRUE:
        expect(tTRUE);
        return new BoolNode(_context, location, true);

    case tFALSE:
        expect(tFALSE);
        return new BoolNode(_context, location, false);

    case '[':
        expect('[');
        if (accept(']'))
        {
            return new FunctionCallNode(_context, location, "Nil", {});
        }
        else
        {
            std::vector<ExpressionNode*> argList;
            argList.push_back(expression());

            while (accept(','))
            {
                argList.push_back(expression());
            }

            expect(']');

            return makeList(_context, location, argList);
        }

    case tINT_LIT:
    {
        Token token = expect(tINT_LIT);
        return new IntNode(_context, location, token.value.number);
    }

    case tSTRING_LIT:
    {
        Token token = expect(tSTRING_LIT);
        return new StringLiteralNode(_context, location, token.value.str);
    }

    case tLIDENT:
    case tUIDENT:
        if (peekType() == tLIDENT && peek2ndType() == '{')
        {
            Token varName = expect(tLIDENT);
            expect('{');
            Token memberName = expect(tLIDENT);
            expect('}');

            return new MemberAccessNode(_context, location, varName.value.str, memberName.value.str);
        }
        else if (peekType() == tLIDENT && peek2ndType() == tDOT_BRACKET)
        {
            Token varName = expect(tLIDENT);
            expect(tDOT_BRACKET);
            ExpressionNode* index = expression();
            expect(']');

            return new FunctionCallNode(_context, location, "arrayAt",
                {new VariableNode(_context, location, varName.value.str), index});
        }
        else
        {
            return new NullaryNode(_context, location, ident());
        }

    default:
    {
        std::stringstream ss;

        ss << "Near line " << location.first_line << ", "
           << "column " << location.first_column << ": "
           << "token " << tokenToString(peekType()) << " cannot start a unary expression.";

        throw LexerError(ss.str());
    }

    }
}

