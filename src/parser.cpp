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
Token nextTokens[2] = { {tNONE, {""}}, {tNONE, {""}} };

// Location of the current token
YYLTYPE yylloc;

// Calls the lexer
int yylex();

void advance()
{
    nextTokens[0] = nextTokens[1];

    if (nextTokens[0].type != tEOF)
    {
        nextTokens[1].type = (TokenType)yylex();
        nextTokens[1].value = yylval;
    }
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
    ProgramNode* node = new ProgramNode;

    while (!accept(tEOF))
    {
        node->append(statement());
    }

    return node;
}

StatementNode* statement()
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
    expect(tDATA);
    Token name = expect(tUIDENT);

    std::vector<std::string> typeParameters;
    while (peekType() == tLIDENT)
    {
        Token token = expect(tLIDENT);
        typeParameters.push_back(token.value.str);
    }

    expect('=');
    ConstructorSpec* constructorSpec = constructor_spec();
    expect(tEOL);

    return new DataDeclaration(name.value.str, typeParameters, constructorSpec);
}

StatementNode* type_alias_declaration()
{
    expect(tTYPE);
    Token name = expect(tUIDENT);
    expect('=');
    TypeName* typeName = type();
    expect(tEOL);

    return new TypeAliasNode(name.value.str, typeName);
}

StatementNode* function_definition()
{
    expect(tDEF);
    std::string name = ident();
    ParamList* params = parameters();
    TypeDecl* typeDecl = accept(tDCOLON) ? type_declaration() : nullptr;
    expect('=');
    StatementNode* body = suite();

    return new FunctionDefNode(name, body, params, typeDecl);
}

StatementNode* for_statement()
{
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
    expect(tFOREIGN);
    std::string name = ident();
    ParamList* params = parameters();
    expect(tDCOLON);
    TypeDecl* typeDecl = type_declaration();
    expect(tEOL);

    return new ForeignDeclNode(name, params, typeDecl);
}

StatementNode* match_statement()
{
    expect(tLET);
    Token constructor = expect(tUIDENT);
    ParamList* params = parameters();
    expect('=');
    ExpressionNode* body = expression();
    expect(tEOL);

    return new MatchNode(constructor.value.str, params, body);
}

StatementNode* return_statement()
{
    expect(tRETURN);
    ExpressionNode* value = expression();
    expect(tEOL);

    return new ReturnNode(value);
}

StatementNode* struct_declaration()
{
    expect(tSTRUCT);
    Token name = expect(tUIDENT);
    expect('=');
    MemberList* memberList = members();

    return new StructDefNode(name.value.str, memberList);
}

StatementNode* while_statement()
{
    expect(tWHILE);
    ExpressionNode* condition = expression();
    expect(tDO);
    StatementNode* body = suite();

    return new WhileNode(condition, body);
}

StatementNode* assignment_statement()
{
    Token token = expect(tLIDENT);
    std::string lhs = token.value.str;

    if (accept((TokenType)'='))
    {
        ExpressionNode* rhs = expression();

        expect(tEOL);

        return new AssignNode(lhs, rhs);
    }

    ArgList argList;
    argList.emplace_back(new VariableNode(lhs));

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
    argList.emplace_back(rhs);

    expect(tEOL);

    return new AssignNode(lhs, new FunctionCallNode(functionName, std::move(argList)));
}

StatementNode* variable_declaration()
{
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

StatementNode* break_statement()
{
    expect(tBREAK);
    expect(tEOL);

    return new BreakNode();
}

//// Miscellaneous /////////////////////////////////////////////////////////////

StatementNode* suite()
{
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

ParamList* parameters()
{
    ParamList* result = new ParamList;
    while (peekType() == tLIDENT)
    {
        Token param = expect(tLIDENT);

        result->push_back(param.value.str);
    }

    return result;
}

std::string ident()
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

TypeDecl* type_declaration()
{
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
    if (peekType() == tUIDENT)
    {
        Token name = expect(tUIDENT);
        TypeName* typeName = new TypeName(name.value.str);

        while (peekType() == tUIDENT || peekType() == tLIDENT || peekType() == '[')
        {
            typeName->append(simple_type());
        }

        return typeName;
    }
    else
    {
        return simple_type();
    }
}

TypeName* simple_type()
{
    if (peekType() == tUIDENT)
    {
        Token typeName = expect(tUIDENT);
        return new TypeName(typeName.value.str);
    }
    else if (peekType() == tLIDENT)
    {
        Token typeName = expect(tLIDENT);
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
    Token name = expect(tUIDENT);

    ConstructorSpec* constructorSpec = new ConstructorSpec(name.value.str);
    while (peekType() == tUIDENT || peekType() == tLIDENT || peekType() == '[')
    {
        constructorSpec->append(simple_type());
    }

    return constructorSpec;
}

 //// Structures ///////////////////////////////////////////////////////////////

MemberList* members()
{
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
    Token name = expect(tLIDENT);
    expect(tDCOLON);
    TypeName* typeName = type();
    expect(tEOL);

    return new MemberDefNode(name.value.str, typeName);
}

//// Expressions ///////////////////////////////////////////////////////////////

ExpressionNode* expression()
{
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
    ExpressionNode* lhs = relational_expression();

    if (accept(tEQUALS))
    {
        return new ComparisonNode(lhs, ComparisonNode::kEqual, relational_expression());
    }
    else if (accept(tNE))
    {
        return new ComparisonNode(lhs, ComparisonNode::kNotEqual, relational_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* relational_expression()
{
    ExpressionNode* lhs = cons_expression();

    if (accept((TokenType)'>'))
    {
        return new ComparisonNode(lhs, ComparisonNode::kGreater, cons_expression());
    }
    else if (accept((TokenType)'<'))
    {
        return new ComparisonNode(lhs, ComparisonNode::kLess, cons_expression());
    }
    else if (accept(tGE))
    {
        return new ComparisonNode(lhs, ComparisonNode::kGreaterOrEqual, cons_expression());
    }
    else if (accept(tLE))
    {
        return new ComparisonNode(lhs, ComparisonNode::kLessOrEqual, cons_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* cons_expression()
{
    ExpressionNode* lhs = additive_expression();

    if (accept((TokenType)':'))
    {
        return new FunctionCallNode("Cons", {lhs, cons_expression()});
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* additive_expression()
{
    ExpressionNode* result = multiplicative_expression();

    while (peekType() == '+' || peekType() == '-')
    {
        if (accept('+'))
        {
            result = new FunctionCallNode("+", {result, multiplicative_expression()});
        }
        else
        {
            expect('-');
            result = new FunctionCallNode("-", {result, multiplicative_expression()});
        }
    }

    return result;
}

ExpressionNode* multiplicative_expression()
{
    ExpressionNode* result = concat_expression();

    while (peekType() == '*' || peekType() == '/' || peekType() == tMOD)
    {
        if (accept('*'))
        {
            result = new FunctionCallNode("*", {result, concat_expression()});
        }
        else if (accept('/'))
        {
            result = new FunctionCallNode("/", {result, concat_expression()});
        }
        else
        {
            expect(tMOD);
            result = new FunctionCallNode("%", {result, concat_expression()});
        }
    }

    return result;
}

ExpressionNode* concat_expression()
{
    ExpressionNode* lhs = negation_expression();

    if (accept(tCONCAT))
    {
        return new FunctionCallNode("concat", {lhs, concat_expression()});
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* negation_expression()
{
    if (accept('-'))
    {
        return new FunctionCallNode("-", {new IntNode(0), func_call_expression()});
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
    if ((peekType() == tLIDENT || peekType() == tUIDENT) && (canStartUnaryExpression(peek2ndType()) || peek2ndType() == '$'))
    {
        std::string functionName = ident();

        ArgList argList;
        while (canStartUnaryExpression(peekType()))
        {
            argList.emplace_back(unary_expression());
        }

        if (peekType() == '$')
        {
            expect('$');
            argList.emplace_back(expression());
        }

        return new FunctionCallNode(functionName, std::move(argList));
    }
    else
    {
        return unary_expression();
    }
}

ExpressionNode* unary_expression()
{
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
            return new FunctionCallNode("Nil", ArgList());
        }
        else
        {
            ArgList argList;
            argList.emplace_back(expression());

            while (accept((TokenType)','))
            {
                argList.emplace_back(expression());
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
        if (peekType() == tLIDENT && peek2ndType() == '{')
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
        std::cerr << "ERROR: Token " << tokenToString(nextTokens[0].type) << " cannot start a unary expression." << std::endl;
        exit(1);
    }
}

