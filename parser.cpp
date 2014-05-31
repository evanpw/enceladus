#include "parser.hpp"
#include "tokens.hpp"

#include <iostream>
#include <boost/lexical_cast.hpp>

//// Lexing machinery //////////////////////////////////////////////////////////

YYSTYPE yylval;

struct Token
{
    TokenType type;
    YYSTYPE value;
};

// Two tokens of look-ahead
Token nextTokens[2] = { {tNONE, ""}, {tNONE, ""} };

// Location of the current token
YYLTYPE yylloc;

// Calls the lexer
int yylex();

void advance()
{
    //std::cerr << "consuming " << tokenToString(nextTokens[0].type) << " (" << nextTokens[0].type << ")" << std::endl;
    nextTokens[0] = nextTokens[1];

    nextTokens[1].type = (TokenType)yylex();
    nextTokens[1].value = yylval;
}

void initialize()
{
    advance();
    advance();
}

//// Parsing machinery /////////////////////////////////////////////////////////

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

    std::cerr << "ERROR: Expected " << tokenToString(t) << ", but got " << tokenToString(nextTokens[0].type) << std::endl;
    exit(1);
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

ProgramNode* parse()
{
    initialize();
    return program();
}

////////////////////////////////////////////////////////////////////////////////
//// Grammar ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//// Statements ////////////////////////////////////////////////////////////////

ProgramNode* program()
{
    //std::cerr << __func__ << std::endl;

    ProgramNode* node = new ProgramNode;

    while (!accept(tEOF))
    {
        node->append(statement());
    }

    return node;
}

StatementNode* statement()
{
    //std::cerr << __func__ << std::endl;

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

    case tLET:
        return match_statement();

    case tRETURN:
        return return_statement();

    case tSTRUCT:
        return struct_declaration();

    case tWHILE:
        return while_statement();

    case tVAR:
        return variable_declaration();

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
        else if (peek2ndType() == tCOLON_EQUAL || peek2ndType() == tDCOLON)
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

StatementNode* if_statement()
{
    //std::cerr << __func__ << std::endl;

    expect(tIF);
    ExpressionNode* condition = expression();
    expect(tTHEN);
    StatementNode* ifBody = suite();

    if (accept(tELSE))
    {
        StatementNode* elseBody = suite();
        return new IfElseNode(condition, ifBody, elseBody);
    }
    else
    {
        return new IfNode(condition, ifBody);
    }
}

StatementNode* data_declaration()
{
    //std::cerr << __func__ << std::endl;

    expect(tDATA);
    Token name = expect(tUIDENT);
    expect('=');
    ConstructorSpec* constructorSpec = constructor_spec();
    expect(tEOL);

    return new DataDeclaration(name.value.str, constructorSpec);
}

StatementNode* type_alias_declaration()
{
    //std::cerr << __func__ << std::endl;

    expect(tTYPE);
    Token name = expect(tUIDENT);
    expect('=');
    TypeName* typeName = type();
    expect(tEOL);

    return new TypeAliasNode(name.value.str, typeName);
}

StatementNode* function_definition()
{
    //std::cerr << __func__ << std::endl;

    expect(tDEF);
    std::string name = ident();
    ParamListNode* params = parameters();
    TypeDecl* typeDecl = accept(tDCOLON) ? type_declaration() : nullptr;
    expect('=');
    StatementNode* body = suite();

    return new FunctionDefNode(name, body, params, typeDecl);
}

StatementNode* for_statement()
{
    //std::cerr << __func__ << std::endl;

    expect(tFOR);
    Token loopVar = expect(tLIDENT);
    expect(tIN);
    ExpressionNode* listExpression = expression();
    expect(tDO);
    StatementNode* body = suite();

    return makeForNode(loopVar.value.str, listExpression, body);
}

StatementNode* foreign_declaration()
{
    //std::cerr << __func__ << std::endl;

    expect(tFOREIGN);
    std::string name = ident();
    ParamListNode* params = parameters();
    expect(tDCOLON);
    TypeDecl* typeDecl = type_declaration();
    expect(tEOL);

    return new ForeignDeclNode(name, params, typeDecl);
}

StatementNode* match_statement()
{
    //std::cerr << __func__ << std::endl;

    expect(tLET);
    Token constructor = expect(tUIDENT);
    ParamListNode* params = parameters();
    expect('=');
    ExpressionNode* body = expression();
    expect(tEOL);

    return new MatchNode(constructor.value.str, params, body);
}

StatementNode* return_statement()
{
    //std::cerr << __func__ << std::endl;

    expect(tRETURN);
    ExpressionNode* value = expression();
    expect(tEOL);

    return new ReturnNode(value);
}

StatementNode* struct_declaration()
{
    //std::cerr << __func__ << std::endl;

    expect(tSTRUCT);
    Token name = expect(tUIDENT);
    expect('=');
    MemberList* memberList = members();

    return new StructDefNode(name.value.str, memberList);
}

StatementNode* while_statement()
{
    //std::cerr << __func__ << std::endl;

    expect(tWHILE);
    ExpressionNode* condition = expression();
    expect(tDO);
    StatementNode* body = suite();

    return new WhileNode(condition, body);
}

StatementNode* assignment_statement()
{
    //std::cerr << __func__ << std::endl;

    AssignableNode* lhs = assignable();

    if (accept((TokenType)'='))
    {
        ExpressionNode* rhs = expression();

        expect(tEOL);

        return new AssignNode(lhs, rhs);
    }

    ArgList* argList = new ArgList;
    argList->emplace_back(lhs);

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
    argList->emplace_back(rhs);

    expect(tEOL);

    return new AssignNode(lhs->clone(), new FunctionCallNode(functionName, argList));
}

StatementNode* variable_declaration()
{
    //std::cerr << __func__ << std::endl;

    if (peekType() == tLIDENT)
    {
        Token varName = expect(tLIDENT);
        TypeName* varType = accept(tDCOLON) ? type() : nullptr;
        expect(tCOLON_EQUAL);
        ExpressionNode* value = expression();
        expect(tEOL);

        return new LetNode(varName.value.str, varType, value);
    }
    else
    {
        expect(tVAR);
        Token varName = expect(tLIDENT);
        TypeName* varType = accept(tDCOLON) ? type() : nullptr;
        expect('=');
        ExpressionNode* value = expression();
        expect(tEOL);

        return new LetNode(varName.value.str, varType, value);
    }
}

//// Miscellaneous /////////////////////////////////////////////////////////////

AssignableNode* assignable()
{
    //std::cerr << __func__ << std::endl;

    Token token = expect(tLIDENT);
    //std::cerr << "assignable: " << token.value.str << std::endl;

    if (peekType() == '{')
    {
        expect('{');
        Token memberName = expect(tLIDENT);
        expect('}');

        return new MemberAccessNode(token.value.str, memberName.value.str);
    }

    return new VariableNode(token.value.str);
}

StatementNode* suite()
{
    //std::cerr << __func__ << std::endl;

    if (accept(tEOL))
    {
        expect(tINDENT);

        BlockNode* block = new BlockNode;
        while (peekType() != tDEDENT)
        {
            block->append(statement());
        }

        expect(tDEDENT);

        return block;
    }
    else
    {
        return statement();
    }
}

ParamListNode* parameters()
{
    //std::cerr << __func__ << std::endl;

    ParamListNode* result = new ParamListNode;
    while (peekType() == tLIDENT)
    {
        Token param = expect(tLIDENT);

        result->append(param.value.str);
    }

    return result;
}

std::string ident()
{
    //std::cerr << __func__ << std::endl;

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

    //std::cerr << "ident: " << name.value.str << std::endl;

    return name.value.str;
}

//// Types /////////////////////////////////////////////////////////////////////

TypeDecl* type_declaration()
{
    //std::cerr << __func__ << std::endl;

    TypeDecl* typeDecl = new TypeDecl;
    typeDecl->emplace_back(type());

    while (accept(tRARROW))
    {
        typeDecl->emplace_back(type());
    }

     return typeDecl;
}

TypeName* type()
{
    //std::cerr << __func__ << std::endl;

    if (peekType() == tUIDENT)
    {
        Token typeName = expect(tUIDENT);
        return new TypeName(typeName.value.str);
    }
    else
    {
        expect('[');
        TypeName* internalType = type();
        expect(']');

        TypeName* typeName = new TypeName("List");
        typeName->append(internalType);

        return typeName;
    }
}

ConstructorSpec* constructor_spec()
{
    //std::cerr << __func__ << std::endl;

    Token name = expect(tUIDENT);

    ConstructorSpec* constructorSpec = new ConstructorSpec(name.value.str);
    while (peekType() == tUIDENT || peekType() == '[')
    {
        constructorSpec->append(type());
    }

    return constructorSpec;
}

 //// Structures ///////////////////////////////////////////////////////////////

MemberList* members()
{
    //std::cerr << __func__ << std::endl;

    MemberList* memberList = new MemberList();

    if (accept(tEOL))
    {
        expect(tINDENT);

        while (peekType() != tDEDENT)
        {
            memberList->emplace_back(member_definition());
        }

        expect(tDEDENT);
    }
    else
    {
        memberList->emplace_back(member_definition());
    }

    return memberList;
}

MemberDefNode* member_definition()
{
    //std::cerr << __func__ << std::endl;

    Token name = expect(tLIDENT);
    expect(tDCOLON);
    TypeName* typeName = type();
    expect(tEOL);

    return new MemberDefNode(name.value.str, typeName);
}

//// Expressions ///////////////////////////////////////////////////////////////

ExpressionNode* expression()
{
    //std::cerr << __func__ << std::endl;

    ExpressionNode* lhs = and_expression();

    if (accept(tOR))
    {
        return new LogicalNode(lhs, LogicalNode::kOr, expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* and_expression()
{
    //std::cerr << __func__ << std::endl;

    ExpressionNode* lhs = equality_expression();

    if (accept(tAND))
    {
        return new LogicalNode(lhs, LogicalNode::kAnd, and_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* equality_expression()
{
    //std::cerr << __func__ << std::endl;

    ExpressionNode* lhs = relational_expression();

    if (accept(tEQUALS))
    {
        return new ComparisonNode(lhs, ComparisonNode::kEqual, equality_expression());
    }
    else if (accept(tNE))
    {
        return new ComparisonNode(lhs, ComparisonNode::kNotEqual, equality_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* relational_expression()
{
    //std::cerr << __func__ << std::endl;

    ExpressionNode* lhs = cons_expression();

    if (accept((TokenType)'>'))
    {
        return new ComparisonNode(lhs, ComparisonNode::kGreater, relational_expression());
    }
    else if (accept((TokenType)'<'))
    {
        return new ComparisonNode(lhs, ComparisonNode::kLess, relational_expression());
    }
    else if (accept(tGE))
    {
        return new ComparisonNode(lhs, ComparisonNode::kGreaterOrEqual, relational_expression());
    }
    else if (accept(tLE))
    {
        return new ComparisonNode(lhs, ComparisonNode::kLessOrEqual, relational_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* cons_expression()
{
    //std::cerr << __func__ << std::endl;

    ExpressionNode* lhs = additive_expression();

    if (accept((TokenType)':'))
    {
        ArgList* argList = new ArgList;
        argList->emplace_back(lhs);
        argList->emplace_back(cons_expression());
        return new FunctionCallNode("Cons", argList);
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* additive_expression()
{
    //std::cerr << __func__ << std::endl;

    ExpressionNode* lhs = multiplicative_expression();

    if (accept((TokenType)'+'))
    {
        ArgList* argList = new ArgList;
        argList->emplace_back(lhs);
        argList->emplace_back(additive_expression());
        return new FunctionCallNode("+", argList);
    }
    else if (accept((TokenType)'-'))
    {
        ArgList* argList = new ArgList;
        argList->emplace_back(lhs);
        argList->emplace_back(additive_expression());
        return new FunctionCallNode("-", argList);
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* multiplicative_expression()
{
    //std::cerr << __func__ << std::endl;

    ExpressionNode* lhs = concat_expression();

    if (accept((TokenType)'*'))
    {
        ArgList* argList = new ArgList;
        argList->emplace_back(lhs);
        argList->emplace_back(multiplicative_expression());
        return new FunctionCallNode("*", argList);
    }
    else if (accept((TokenType)'/'))
    {
        ArgList* argList = new ArgList;
        argList->emplace_back(lhs);
        argList->emplace_back(multiplicative_expression());
        return new FunctionCallNode("/", argList);
    }
    else if (accept(tMOD))
    {
        ArgList* argList = new ArgList;
        argList->emplace_back(lhs);
        argList->emplace_back(multiplicative_expression());
        return new FunctionCallNode("%", argList);
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* concat_expression()
{
    //std::cerr << __func__ << std::endl;

    ExpressionNode* lhs = negation_expression();

    if (accept(tCONCAT))
    {
        ArgList* argList = new ArgList;
        argList->emplace_back(lhs);
        argList->emplace_back(concat_expression());
        return new FunctionCallNode("concat", argList);
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* negation_expression()
{
    //std::cerr << __func__ << std::endl;
    if (accept('-'))
    {
        ArgList* argList = new ArgList;
        argList->emplace_back(new IntNode(0));
        argList->emplace_back(expression());
        return new FunctionCallNode("-", argList);
    }
    else
    {
        return func_call_expression();
    }
}

bool canStartUnaryExpression(TokenType t)
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

ExpressionNode* func_call_expression()
{
    //std::cerr << __func__ << std::endl;

    if ((peekType() == tLIDENT || peekType() == tUIDENT) && (canStartUnaryExpression(peek2ndType()) || peek2ndType() == '$'))
    {
        std::string functionName = ident();

        //std::cerr << "function call: " << functionName << std::endl;

        ArgList* argList = new ArgList;
        while (canStartUnaryExpression(peekType()))
        {
            argList->emplace_back(unary_expression());
        }

        //std::cerr << "end args: " << functionName << std::endl;

        if (peekType() == '$')
        {
            expect('$');
            argList->emplace_back(expression());
        }

        return new FunctionCallNode(functionName, argList);
    }
    else
    {
        return unary_expression();
    }
}

ExpressionNode* unary_expression()
{
    //std::cerr << __func__ << std::endl;

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
        return new BoolNode(true);

    case tFALSE:
        expect(tFALSE);
        return new BoolNode(false);

    case '[':
        expect('[');
        if (accept((TokenType)']'))
        {
            return new FunctionCallNode("Nil", new ArgList);
        }
        else
        {
            ArgList* argList = new ArgList;
            argList->emplace_back(expression());

            while (accept((TokenType)','))
            {
                argList->emplace_back(expression());
            }

            expect(']');

            return makeList(argList);
        }

    case tINT_LIT:
    {
        Token token = expect(tINT_LIT);
        return new IntNode(token.value.number);
    }

    case tSTRING_LIT:
    {
        Token token = expect(tSTRING_LIT);
        return makeString(token.value.str);
    }

    case tLIDENT:
    case tUIDENT:
        if (peekType() == tUIDENT && peek2ndType() == '{')
        {
            Token name = expect(tUIDENT);
            expect('{');
            expect('}');

            return new StructInitNode(name.value.str);
        }
        else if (peekType() == tLIDENT && peek2ndType() == '{')
        {
            Token varName = expect(tLIDENT);
            expect('{');
            Token memberName = expect(tLIDENT);
            expect('}');

            return new MemberAccessNode(varName.value.str, memberName.value.str);
        }
        else
        {
            return new NullaryNode(ident());
        }

    default:
        std::cerr << "ERROR: Token " << tokenToString(nextTokens[0].type) << " cannot start an expression." << std::endl;
        exit(1);
    }
}

