#include "parser/parser.hpp"
#include "exceptions.hpp"
#include "parser/tokens.hpp"

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

    ss << location.filename << ":" << location.first_line << ":" << location.first_column
       << ": expected " << tokenToString(t) << ", but got "
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

    while (!accept(tEND))
    {
        StatementNode* child = statement();

        if (child)
            node->children.push_back(child);
    }

    _context->setRoot(node);

    return node;
}

StatementNode* Parser::statement()
{
    if (accept(tEOL))
        return nullptr;

    switch(peekType())
    {
    case tPASS:
        return pass_statement();

    case tIF:
        return if_statement();

    case tASSERT:
        return assert_statement();

    case tENUM:
        return enum_declaration();

    case tTYPE:
        return type_alias();

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

    case tBREAK:
        return break_statement();

    case tCONTINUE:
        return continue_statement();

    case tIMPL:
        return implementation_block();

    case tTRAIT:
        return trait_definition();

    case tLIDENT:
        if (peek2ndType() == tCOLON_EQUAL || peek2ndType() == ':')
        {
            return variable_declaration();
        }

        // Else fallthrough

    default:
        return assign_or_expr();
    }
}

PassNode* Parser::pass_statement()
{
    YYLTYPE location = getLocation();
    expect(tPASS);
    expect(tEOL);

    return new PassNode(_context, location);
}

/// if_statement
///     : IF ( expression | let_expression ) { ELIF ( expression | let_expression ) } [ ELSE suite ]
StatementNode* Parser::if_helper(const YYLTYPE& location)
{
    ExpressionNode* condition;
    if (peekType() == tLET)
    {
        condition = let_expression();
    }
    else
    {
        condition = expression();
    }

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
        return new IfElseNode(_context, location, condition, ifBody);
    }
}

StatementNode* Parser::if_statement()
{
    YYLTYPE location = getLocation();

    expect(tIF);
    return if_helper(location);
}

/// assert_statement
///     : ASSERT expression EOL
AssertNode* Parser::assert_statement()
{
    YYLTYPE location = getLocation();

    expect(tASSERT);
    ExpressionNode* condition = expression();
    expect(tEOL);

    return new AssertNode(_context, location, condition);
}

/// enum_declaration
///     : ENUM UIDENT constrained_type_params EOL INDENT constructor_spec { constructor_spec } DEDENT
///     : ENUM UIDENT constrained_type_params '=' constructor_spec EOL
EnumDeclaration* Parser::enum_declaration()
{
    YYLTYPE location = getLocation();

    expect(tENUM);
    Token name = expect(tUIDENT);

    std::vector<TypeParam> typeParameters = constrained_type_params();

    if (accept('='))
    {
        ConstructorSpec* spec = constructor_spec();
        return new EnumDeclaration(_context, location, name.value.str, std::move(typeParameters), {spec});
    }
    else
    {
        expect(tEOL);
        expect(tINDENT);

        std::vector<ConstructorSpec*> specs;
        specs.push_back(constructor_spec());
        while (!accept(tDEDENT))
        {
            specs.push_back(constructor_spec());
        }

        return new EnumDeclaration(_context, location, name.value.str, std::move(typeParameters), specs);
    }
}

TypeAliasNode* Parser::type_alias()
{
    YYLTYPE location = getLocation();

    expect(tTYPE);
    Token name = expect(tUIDENT);
    expect('=');
    TypeName* typeName = type();
    expect(tEOL);

    return new TypeAliasNode(_context, location, name.value.str, typeName);
}

/// function_definition
///     : DEF ident params_and_types [ where_clause ] suite
FunctionDefNode* Parser::function_definition()
{
    YYLTYPE location = getLocation();

    expect(tDEF);
    std::string name = ident();
    std::pair<std::vector<std::string>, TypeName*> paramsAndTypes = params_and_types();

    std::vector<TypeParam> typeParams = where_clause();

    StatementNode* body = suite();
    return new FunctionDefNode(_context, location, name, body, std::move(typeParams), paramsAndTypes.first, paramsAndTypes.second);
}

ForNode* Parser::for_statement()
{
    YYLTYPE location = getLocation();

    expect(tFOR);
    Token loopVar = expect(tLIDENT);

    expect(tIN);
    ExpressionNode* iterableExpression = expression();
    StatementNode* body = suite();

    return new ForNode(_context, location, loopVar.value.str, iterableExpression, body);
}

ForeignDeclNode* Parser::foreign_declaration()
{
    YYLTYPE location = getLocation();

    expect(tFOREIGN);
    std::string name = ident();
    std::vector<std::string> typeParams = type_params();
    std::pair<std::vector<std::string>, TypeName*> paramsAndTypes = params_and_types();
    expect(tEOL);

    return new ForeignDeclNode(_context, location, name, typeParams, paramsAndTypes.first, paramsAndTypes.second);
}

ForeverNode* Parser::forever_statement()
{
    YYLTYPE location = getLocation();

    expect(tFOREVER);
    StatementNode* body = suite();

    return new ForeverNode(_context, location, body);
}

/// let_expression
///     : LET UIDENT parameters COLON_EQUAL expression
LetNode* Parser::let_expression()
{
    YYLTYPE location = getLocation();

    expect(tLET);
    Token constructor = expect(tUIDENT);
    std::vector<std::string> params = parameters();
    expect(tCOLON_EQUAL);
    ExpressionNode* body = expression();

    return new LetNode(_context, location, constructor.value.str, params, body);
}

/// let_statement
///     : let_expression EOL
LetNode* Parser::let_statement()
{
    YYLTYPE location = getLocation();
    LetNode* letNode = let_expression();
    letNode->isExpression = false;
    expect(tEOL);

    return letNode;
}

/// match_statement: MATCH expression EOL match_body
/// match_body: INDENT match_arm { match_arm } DEDENT
MatchNode* Parser::match_statement()
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

    return new MatchNode(_context, location, expr, std::move(arms));
}

/// match_arm
///     : UIDENT parameters ( '=>' statement | EOL INDENT statement_list DEDENT)
MatchArm* Parser::match_arm()
{
    YYLTYPE location = getLocation();
    Token constructor = expect(tUIDENT);
    std::vector<std::string> params = parameters();

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
        expect(tDARROW);
        StatementNode* body = statement();
        return new MatchArm(_context, location, constructor.value.str, params, body);
    }
}

/// return_statement
///     : RETURN [ expression ] EOL
ReturnNode* Parser::return_statement()
{
    YYLTYPE location = getLocation();

    expect(tRETURN);

    if (accept(tEOL))
    {
        return new ReturnNode(_context, location);
    }
    else
    {
        ExpressionNode* value = expression();
        expect(tEOL);
        return new ReturnNode(_context, location, value);
    }
}

/// struct_declaration
///     : STRUCT UIDENT constrained_type_params [ where_clause ] members
StructDefNode* Parser::struct_declaration()
{
    YYLTYPE location = getLocation();

    expect(tSTRUCT);
    Token name = expect(tUIDENT);

    std::vector<TypeParam> typeParams = constrained_type_params();
    std::vector<TypeParam> whereClause = where_clause();
    std::vector<MemberDefNode*> memberList = members();

    return new StructDefNode(_context, location, name.value.str, std::move(memberList), std::move(typeParams), std::move(whereClause));
}

/// while_statement
///     : WHILE ( expression | let_expression ) suite
WhileNode* Parser::while_statement()
{
    YYLTYPE location = getLocation();

    expect(tWHILE);
    ExpressionNode* condition = (peekType() == tLET) ? let_expression() : expression();
    StatementNode* body = suite();

    return new WhileNode(_context, location, condition, body);
}

StatementNode* Parser::assign_or_expr()
{
    YYLTYPE location = getLocation();

    ExpressionNode* lhs = expression();

    if (accept('='))
    {
        ExpressionNode* rhs = expression();

        expect(tEOL);

        return new AssignNode(_context, location, lhs, rhs);
    }
    else if (peekType() == tPLUS_EQUAL ||
             peekType() == tMINUS_EQUAL ||
             peekType() == tTIMES_EQUAL ||
             peekType() == tDIV_EQUAL ||
             peekType() == tREM_EQUAL)
    {
        BinopNode::Op op;
        switch (peekType())
        {
        case tPLUS_EQUAL:
            expect(tPLUS_EQUAL);
            op = BinopNode::kAdd;
            break;

        case tMINUS_EQUAL:
            expect(tMINUS_EQUAL);
            op = BinopNode::kSub;
            break;

        case tTIMES_EQUAL:
            expect(tTIMES_EQUAL);
            op = BinopNode::kMul;
            break;

        case tDIV_EQUAL:
            expect(tDIV_EQUAL);
            op = BinopNode::kDiv;
            break;

        case tREM_EQUAL:
            expect(tREM_EQUAL);
            op = BinopNode::kRem;
            break;

        default:
            assert(false);
        }

        ExpressionNode* rhs = expression();

        expect(tEOL);

        BinopNode* result = new BinopNode(_context, location, lhs, op, rhs);
        return new AssignNode(_context, location, lhs, result);
    }
    else
    {
        expect(tEOL);
        return lhs;
    }
}

/// variable_declaration
///     : LIDENT COLON_EQUAL expression EOL
VariableDefNode* Parser::variable_declaration()
{
    YYLTYPE location = getLocation();

    Token varName = expect(tLIDENT);
    expect(tCOLON_EQUAL);
    ExpressionNode* value = expression();
    expect(tEOL);

    return new VariableDefNode(_context, location, varName.value.str, value);
}

BreakNode* Parser::break_statement()
{
    YYLTYPE location = getLocation();

    expect(tBREAK);
    expect(tEOL);

    return new BreakNode(_context, location);
}

ContinueNode* Parser::continue_statement()
{
    YYLTYPE location = getLocation();

    expect(tCONTINUE);
    expect(tEOL);

    return new ContinueNode(_context, location);
}

/// implementation_block
///     : IMPL type [ FOR type ] [ ':' UIDENT { '+' UIDENT } ] where_clause EOL INDENT [ member { member } DEDENT ]
///
/// member
///     : method_definition
///     | type_alias
ImplNode* Parser::implementation_block()
{
    YYLTYPE location = getLocation();

    expect(tIMPL);

    TypeName* typeName = type();
    TypeName* traitName = nullptr;

    if (accept(tFOR))
    {
        traitName = typeName;
        typeName = type();
    }

    // Implicit where clause syntax: "impl T: Trait2" == "impl T where T: Trait2"
    std::vector<TypeParam> implicitParams;
    if (accept(':'))
    {
        if (!typeName->parameters.empty() || typeName->name.size() > 1)
        {
            std::stringstream ss;

            ss << location.filename << ":" << location.first_line
               << ":" << location.first_column
               << ": implicit where clause can only be applied to type variables";

            throw LexerError(ss.str());
        }

        TypeParam implicitParam(typeName->name);

        implicitParam.constraints.push_back(trait_name());

        while (accept('+'))
        {
            implicitParam.constraints.push_back(trait_name());
        }

        implicitParams.push_back(implicitParam);
    }

    std::vector<TypeParam> typeParams = where_clause();
    if (!implicitParams.empty())
    {
        typeParams.push_back(implicitParams[0]);
    }

    expect(tEOL);

    std::vector<StatementNode*> members;
    if (accept(tINDENT))
    {
        while (true)
        {
            if (peekType() == tDEF)
            {
                MethodDefNode* method = method_definition();
                members.push_back(method);
            }
            else if (peekType() == tTYPE)
            {
                if (!traitName)
                {
                    std::stringstream ss;

                    ss << location.filename << ":" << location.first_line
                       << ":" << location.first_column
                       << ": only trait implementations may have associated types";

                    throw LexerError(ss.str());
                }

                TypeAliasNode* associatedType = type_alias();
                members.push_back(associatedType);
            }
            else
            {
                break;
            }
        }

        expect(tDEDENT);
    }

    return new ImplNode(_context, location, std::move(typeParams), typeName, std::move(members), traitName);
}

/// method_definition
///     : DEF ident params_and_types where_clause suite
MethodDefNode* Parser::method_definition()
{
    YYLTYPE location = getLocation();

    expect(tDEF);
    std::string name = ident();
    auto paramsAndTypes = params_and_types(true);
    auto typeParams = where_clause();

    StatementNode* body = suite();
    return new MethodDefNode(_context, location, name, body, std::move(typeParams), paramsAndTypes.first, paramsAndTypes.second);
}

/// trait_definition
///     : TRAIT UIDENT type_params EOL [ INDENT trait_member { trait_member } DEDENT ]
TraitDefNode* Parser::trait_definition()
{
    YYLTYPE location = getLocation();

    expect(tTRAIT);

    Token traitName = expect(tUIDENT);
    auto typeParams = type_params();

    expect(tEOL);

    std::vector<TraitItem*> members;

    if (accept(tINDENT))
    {
        while (!accept(tDEDENT))
        {
            members.push_back(trait_member());
        }
    }

    return new TraitDefNode(_context, location, traitName.value.str, std::move(typeParams), std::move(members));
}

/// trait_member
///     : trait_method
///     | associated_type
TraitItem* Parser::trait_member()
{
    if (peekType() == tDEF)
    {
        return trait_method();
    }
    else
    {
        return associated_type();
    }
}

/// trait_method
///     : DEF ident params_and_types EOL
TraitMethodNode* Parser::trait_method()
{
    YYLTYPE location = getLocation();

    expect(tDEF);
    Token methodName = expect(tLIDENT);
    auto paramsAndTypes = params_and_types(true);
    expect(tEOL);

    return new TraitMethodNode(_context, location, methodName.value.str, std::move(paramsAndTypes.first), paramsAndTypes.second);
}

/// associated_type
///     TYPE constrained_type_param EOL
AssociatedTypeNode* Parser::associated_type()
{
    YYLTYPE location = getLocation();

    expect(tTYPE);
    TypeParam type = constrained_type_param();
    expect(tEOL);

    return new AssociatedTypeNode(_context, location, type);
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

    if (accept('('))
    {
        Token param = expect(tLIDENT);
        result.push_back(param.value.str);

        while (accept(','))
        {
            Token param = expect(tLIDENT);
            result.push_back(param.value.str);
        }

        expect(')');
    }

    return result;
}

std::string Parser::ident()
{
    YYLTYPE location = getLocation();

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
        std::stringstream ss;

        ss << location.filename << ":" << location.first_line << ":" << location.first_column
           << ": expected identifier, but got "
           << tokenToString(nextTokens[0].type);

        throw LexerError(ss.str());
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
///     : UIDENT [ '<' type { ',' type } '>' ]
///     | simple_type
TypeName* Parser::constructed_type()
{
    YYLTYPE location = getLocation();

    if (peekType() == tUIDENT)
    {
        Token name = expect(tUIDENT);
        TypeName* typeName = new TypeName(_context, location, name.value.str);

        if (accept('<'))
        {
            typeName->parameters.push_back(type());
            while (accept(','))
            {
                typeName->parameters.push_back(type());
            }

            expect('>');
        }

        return typeName;
    }
    else
    {
        return simple_type();
    }
}

/// simple_type
///     : UIDENT
///     | '[' type ']'
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

/// trait_name
///     : UIDENT [ '<' type {',' type } '>' ]
TypeName* Parser::trait_name()
{
    YYLTYPE location = getLocation();

    Token name = expect(tUIDENT);
    TypeName* typeName = new TypeName(_context, location, name.value.str);

    if (accept('<'))
    {
        typeName->parameters.push_back(type());
        while (accept(','))
        {
            typeName->parameters.push_back(type());
        }

        expect('>');
    }

    return typeName;
}

/// constructor_spec
///     : UIDENT [ '(' type { ',' type } ')' ]
ConstructorSpec* Parser::constructor_spec()
{
    YYLTYPE location = getLocation();
    Token name = expect(tUIDENT);

    ConstructorSpec* constructorSpec = new ConstructorSpec(_context, location, name.value.str);
    if (accept('('))
    {
        constructorSpec->members.push_back(type());
        while (accept(','))
        {
            constructorSpec->members.push_back(type());
        }

        expect(')');
    }

    expect(tEOL);

    return constructorSpec;
}

std::pair<std::string, TypeName*> Parser::param_and_type()
{
    Token param = expect(tLIDENT);
    expect(':');
    TypeName* typeName = type();

    return {param.value.str, typeName};
}

/// params_and_types
///     : '(' [ param_and_type { ',' param_and_type } ] ')' RARROW type
///
/// method_params_and_types
///     : '(' [ LIDENT [ ': type ] { ',' param_and_type } ] ')' RARROW type
std::pair<std::vector<std::string>, TypeName*> Parser::params_and_types(bool isMethod)
{
    YYLTYPE location = getLocation();

    expect('(');

    std::vector<std::string> params;
    TypeName* typeName = new TypeName(_context, location, "Function");

    if (peekType() == tLIDENT)
    {
        if (peek2ndType() == ':' || !isMethod)
        {
            std::pair<std::string, TypeName*> param_type = param_and_type();
            params.push_back(param_type.first);
            typeName->parameters.push_back(param_type.second);
        }
        else
        {
            Token param = expect(tLIDENT);
            params.push_back(param.value.str);
            typeName->parameters.push_back(new TypeName(_context, location, "Self"));
        }

        while (accept(','))
        {
            std::pair<std::string, TypeName*> param_type = param_and_type();
            params.push_back(param_type.first);
            typeName->parameters.push_back(param_type.second);
        }
    }

    expect(')');

    // Return type
    if (accept(tRARROW))
    {
        typeName->parameters.push_back(type());
    }
    else
    {
        typeName->parameters.push_back(new TypeName(_context, location, "Unit"));
    }

    return {params, typeName};
}

std::vector<std::string> Parser::type_params()
{
    if (peekType() != '<')
        return {};

    std::vector<std::string> result;

    expect('<');

    Token typeVar = expect(tUIDENT);
    result.push_back(typeVar.value.str);
    while (accept(','))
    {
        Token typeVar = expect(tUIDENT);
        result.push_back(typeVar.value.str);
    }

    expect('>');

    return result;
}

/// constrained_type_params
///     : '<' constrained_type_param { ',' constrained_type_param } '>'
///     | /* empty */
std::vector<TypeParam> Parser::constrained_type_params()
{
    std::vector<TypeParam> result;

    if (peekType() != '<')
        return result;

    expect('<');

    result.push_back(constrained_type_param());

    while (accept(','))
    {
        result.push_back(constrained_type_param());
    }

    expect('>');

    return result;
}

/// where_clause
///     : WHERE constrained_type_param { ',' constrained_type_param }
///     | /* empty */
std::vector<TypeParam> Parser::where_clause()
{
    std::vector<TypeParam> result;

    if (!accept(tWHERE))
        return result;

    result.push_back(constrained_type_param());

    while (accept(','))
    {
        result.push_back(constrained_type_param());
    }

    return result;
}

/// constrained_type_param
///     : UIDENT [':' trait_name { '+' trait_name }]
TypeParam Parser::constrained_type_param()
{
    Token typeVar = expect(tUIDENT);

    TypeParam result(typeVar.value.str);

    if (accept(':'))
    {
        result.constraints.push_back(trait_name());

        while (accept('+'))
        {
            result.constraints.push_back(trait_name());
        }
    }

    return result;
}

//// Structures ///////////////////////////////////////////////////////////////

std::vector<MemberDefNode*> Parser::members()
{
    std::vector<MemberDefNode*> memberList;

    expect(tEOL);
    expect(tINDENT);

    while (peekType() != tDEDENT)
    {
        memberList.push_back(member_definition());
    }

    expect(tDEDENT);

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
    YYLTYPE location = getLocation();
    ExpressionNode* lhs = and_expression();

    if (accept(tOR))
    {
        return new LogicalNode(_context, location, lhs, LogicalNode::kOr, expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::and_expression()
{
    YYLTYPE location = getLocation();
    ExpressionNode* lhs = equality_expression();

    if (accept(tAND))
    {
        return new LogicalNode(_context, location, lhs, LogicalNode::kAnd, and_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::equality_expression()
{
    YYLTYPE location = getLocation();
    ExpressionNode* lhs = relational_expression();

    if (accept(tEQUALS))
    {
        return new ComparisonNode(_context, location, lhs, ComparisonNode::kEqual, relational_expression());
    }
    else if (accept(tNE))
    {
        return new ComparisonNode(_context, location, lhs, ComparisonNode::kNotEqual, relational_expression());
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::relational_expression()
{
    YYLTYPE location = getLocation();
    ExpressionNode* lhs = range_expression();

    if (accept('>'))
    {
        return new ComparisonNode(_context, location, lhs, ComparisonNode::kGreater, range_expression());
    }
    else if (accept('<'))
    {
        return new ComparisonNode(_context, location, lhs, ComparisonNode::kLess, range_expression());
    }
    else if (accept(tGE))
    {
        return new ComparisonNode(_context, location, lhs, ComparisonNode::kGreaterOrEqual, range_expression());
    }
    else if (accept(tLE))
    {
        return new ComparisonNode(_context, location, lhs, ComparisonNode::kLessOrEqual, range_expression());
    }
    else
    {
        return lhs;
    }
}

// range_expression
//     : additive_expression [ ( TO | TIL) additive_expression ]
ExpressionNode* Parser::range_expression()
{
    YYLTYPE location = getLocation();
    ExpressionNode* lhs = additive_expression();

    if (accept(tTO))
    {
        ExpressionNode* rhs = additive_expression();
        return new FunctionCallNode(_context, location, "inclusiveRange", {lhs, rhs});
    }
    else if (accept(tTIL))
    {
        ExpressionNode* rhs = additive_expression();
        return new FunctionCallNode(_context, location, "range", {lhs, rhs});
    }
    else
    {
        return lhs;
    }
}

ExpressionNode* Parser::additive_expression()
{
    YYLTYPE location = getLocation();
    ExpressionNode* result = multiplicative_expression();

    while (peekType() == '+' || peekType() == '-')
    {
        if (accept('+'))
        {
            result = new BinopNode(_context, location, result, BinopNode::kAdd, multiplicative_expression());
        }
        else
        {
            expect('-');
            result = new BinopNode(_context, location, result, BinopNode::kSub, multiplicative_expression());
        }
    }

    return result;
}

ExpressionNode* Parser::multiplicative_expression()
{
    YYLTYPE location = getLocation();
    ExpressionNode* result = negation_expression();

    while (peekType() == '*' || peekType() == '/' || peekType() == '%')
    {
        if (accept('*'))
        {
            result = new BinopNode(_context, location, result, BinopNode::kMul, negation_expression());
        }
        else if (accept('/'))
        {
            result = new BinopNode(_context, location, result, BinopNode::kDiv, negation_expression());
        }
        else
        {
            expect('%');
            result = new BinopNode(_context, location, result, BinopNode::kRem, negation_expression());
        }
    }

    return result;
}

/// negation_expression
///     : cast_expression
///     | '-' cast_expression
ExpressionNode* Parser::negation_expression()
{
    YYLTYPE location = getLocation();

    if (accept('-'))
    {
        return new BinopNode(_context, location, new IntNode(_context, getLocation(), 0, ""), BinopNode::kSub, cast_expression());
    }
    else
    {
        return cast_expression();
    }
}

/// cast_expression
///     : method_member_idx_expression AS type
///     | method_member_idx_expression
ExpressionNode* Parser::cast_expression()
{
    YYLTYPE location = getLocation();

    ExpressionNode* expr = method_member_idx_expression();
    if (accept(tAS))
    {
        TypeName* typeName = type();
        return new CastNode(_context, location, expr, typeName);
    }
    else
    {
        return expr;
    }
}

/// method_member_idx_expression
///     : func_call_expression
///     | method_member_idx_expression '.' LIDENT
///     | method_member_idx_expression '.' LIDENT '(' [ expression ] { ',' expression } ] ')'
///     | method_member_idx_expression '.' LIDENT '$' expression
///     | method_member_idx_expression '[' expression ']'
ExpressionNode* Parser::method_member_idx_expression()
{
    YYLTYPE location = getLocation();

    ExpressionNode* expr = func_call_expression();
    while (true)
    {
        if (accept('.'))
        {
            Token name = expect(tLIDENT);

            if (accept('('))
            {
                std::vector<ExpressionNode*> argList;
                if (!accept(')'))
                {
                    argList.push_back(expression());

                    while (accept(','))
                    {
                        argList.push_back(expression());
                    }

                    expect(')');
                }

                expr = new MethodCallNode(_context, location, expr, name.value.str, std::move(argList));
            }
            else if (accept('$'))
            {
                ExpressionNode* arg = expression();
                expr = new MethodCallNode(_context, location, expr, name.value.str, {arg});
            }
            else
            {
                expr = new MemberAccessNode(_context, location, expr, name.value.str);
            }
        }
        else if (accept('['))
        {
            ExpressionNode* index = expression();
            expect(']');

            expr = new IndexNode(_context, location, expr, index);
        }
        else
        {
            break;
        }
    }

    return expr;
}

/// func_call_expression
///     : ident '$' expression
///     | ident '(' [ expression ] { ',' expression } ] ')'
///     | UIDENT '::' [ expression ] { ',' expression } ] ')'
///     | static_function_call_expression
ExpressionNode* Parser::func_call_expression()
{
    if ((peekType() == tLIDENT || peekType() == tUIDENT) && (peek2ndType() == '(' || peek2ndType() == '$'))
    {
        YYLTYPE location = getLocation();

        std::string functionName = ident();

        std::vector<ExpressionNode*> argList;
        if (accept('$'))
        {
            argList.push_back(expression());
        }
        else
        {
            expect('(');

            if (!accept(')'))
            {
                argList.push_back(expression());

                while (accept(','))
                {
                    argList.push_back(expression());
                }

                expect(')');
            }
        }

        return new FunctionCallNode(_context, location, functionName, std::move(argList));
    }
    else
    {
        return static_function_call_expression();
    }
}

/// static_function_call_expression
///     : type '::' LIDENT '(' [ expression { ',' expression } ] ')'
///     | type '::' LIDENT '$' expression
///     | lambda_expression
ExpressionNode* Parser::static_function_call_expression()
{
    YYLTYPE location = getLocation();

    if (peekType() == tUIDENT)
    {
        if (peek2ndType() != '<' && peek2ndType() != tDCOLON)
        {
            return lambda_expression();
        }
    }
    // Don't allow the [T] abbreviation for list types because of confusion with
    // list literals
    else if (peekType() != '|')
    {
        return lambda_expression();
    }

    TypeName* typeName = type();
    expect(tDCOLON);

    Token functionName = expect(tLIDENT);

    std::vector<ExpressionNode*> argList;

    if (accept('$'))
    {
        argList.push_back(expression());
    }
    else
    {
        expect('(');

        if (!accept(')'))
        {
            argList.push_back(expression());

            while (accept(','))
            {
                argList.push_back(expression());
            }

            expect(')');
        }
    }

    return new FunctionCallNode(_context, location, functionName.value.str, std::move(argList), typeName);
}

ExpressionNode* Parser::lambda_expression()
{
    YYLTYPE location = getLocation();

    if (peekType() == tLIDENT && peek2ndType() == tRARROW)
    {
        Token varName = expect(tLIDENT);
        expect(tRARROW);
        ExpressionNode* body = expression();

        return new LambdaNode(_context, location, varName.value.str, body);
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

            return createList(_context, location, argList);
        }

    case tINT_LIT:
    {
        return integer_literal();
    }

    case tCHAR_LIT:
    {
        return character_literal();
    }

    case tSTRING_LIT:
    {
        Token token = expect(tSTRING_LIT);
        return new StringLiteralNode(_context, location, token.value.str);
    }

    case tLIDENT:
    case tUIDENT:
        return new NullaryNode(_context, location, ident());

    default:
    {
        std::stringstream ss;

        ss << location.filename << ":" << location.first_line << ":" << location.first_column
           << ": expected expression but got " << tokenToString(peekType());

        throw LexerError(ss.str());
    }

    }
}

ExpressionNode* Parser::integer_literal()
{
    YYLTYPE location = getLocation();
    Token token = expect(tINT_LIT);

    std::string text = token.value.str;
    assert(!text.empty());

    // Extract type suffixes
    std::string suffix;
    for (size_t i = 0; i < text.length(); ++i)
    {
        if ((text[i] < '0' || text[i] > '9') && text[i] != '-')
        {
            suffix = text.substr(i);
            text = text.substr(0, i);

            break;
        }
    }

    assert(!text.empty());

    bool negative = (text[0] == '-');

    // Parse the integer, check for out-of-range errors
    int64_t value;
    bool failure = false;
    try
    {
        if ((text[0] == '-' && suffix == "") || suffix == "i")
        {
            value = boost::lexical_cast<int64_t>(text);
        }
        else if (suffix == "u8")
        {
            if (text[0] != '-')
            {
                uint64_t u8value = boost::lexical_cast<uint64_t>(text);

                if (u8value <= std::numeric_limits<uint8_t>::max())
                {
                    value = u8value;
                }
                else
                {
                    failure = true;
                }
            }
            else
            {
                failure = true;
            }
        }
        else if (suffix == "u")
        {
            if (text[0] != '-')
            {
                uint64_t uvalue = boost::lexical_cast<uint64_t>(text);
                value = uvalue;
            }
            else
            {
                failure = true;
            }
        }
        else /* no suffix and no negative sign */
        {
            uint64_t uvalue = boost::lexical_cast<uint64_t>(text);

            // Integers out of the int64 range have an implicit 'u' suffix
            if (uvalue > std::numeric_limits<int64_t>::max())
            {
                if (text[0] != '-')
                {
                    suffix = "u";
                }
                else
                {
                    failure = true;
                }
            }

            value = uvalue;
        }
    }
    catch (boost::bad_lexical_cast&)
    {
        failure = true;
    }

    if (failure)
    {
        std::stringstream ss;

        ss << location.filename << ":" << location.first_line << ":" << location.first_column
           << ": error: integer literal out of range: " << token.value.str;

        throw LexerError(ss.str());
    }

    IntNode* result = new IntNode(_context, location, value, suffix);
    result->negative = negative;
    return result;
}

ExpressionNode* Parser::character_literal()
{
    YYLTYPE location = getLocation();
    Token token = expect(tCHAR_LIT);

    assert(token.value.unsignedInt < 256);

    IntNode* node = new IntNode(_context, location, token.value.unsignedInt, "");
    node->character = true;
    return node;
}