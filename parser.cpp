enum TokenType
{
    tCOLON,
    tDATA,
    tDCOLON,
    tDEF,
    tDIV,
    tDO,
    tELSE,
    tEOF,
    tEOL,
    tFOR,
    tFOREIGN,
    tIF,
    tIN,
    tLET,
    tLIDENT,
    tMINUS,
    tNONE,
    tPLUS,
    tRETURN,
    tSTRUCT,
    tTHEN,
    tTIMES,
    tUIDENT,
    tVAR,
    tWHILE
};

struct Token
{
    TokenType type;
    std::string lexeme;
};

Token nextTokens[2];
void advance();

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

Token expect(TokenType t)
{
    if (nextTokens[0].type == t)
    {
        Token token = nextTokens[0];
        advance();
        return token;
    }

    std::cerr << "ERROR: Expected " << t << ", but got " << nextTokens[0].type << std::endl;
    exit(1);
}

TokenType peekType()
{
    return nextTokens[0].type;
}

TokenType peek2ndType()
{
    return nextTokens[1].type;
}

////////////////////////////////////////////////////////////////////////////////
//// Grammar ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ProgramNode* program()
{
    ProgramNode* node = new ProgramNode;

    while (!accept(tEOF))
    {
        node->append(statement());
    }

    return node;
}

//// Statements ////////////////////////////////////////////////////////////////

StatementNode* statement()
{
    switch(peekType())
    {
    case tIF:
        return if_statement();

    case tDATA:
        return data_declaration();

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
        return struct_statement();

    case tWHILE:
        return while_statement();

    case tVAR:
        return var_declaration();

    case tLIDENT:
        if (peek2ndType() == '{')
        {
            return assignment_statement();
        }
        else if (peek2ndType() == tCOLON_EQUAL || peek2ndType() == tDCOLON)
        {
            return var_declaration();
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
    expect('=');
    ConstructorSpec* constructorSpec = constructor_spec();
    expect(EOL);

    return new DataDeclaration(name.lexeme, constructorSpec);
}

StatementNode* function_definition()
{
    expect(tDEF);
    std::string name = ident();
    ParamListNode* params = parameters();
    TypeDecl* typeDecl = optionalType();
    expect('=');
    StatementNode* body = suit();

    return new FunctionDefNode(name, params, typeDecl, body);
}

StatementNode* for_statement()
{
    expect(tFOR);
    Token loopVar = expect(LIDENT);
    expect(tIN);
    ExpressionNode* listExpression = expression();
    expect(tDO);
    StatementNode* body = suit();

    return makeForNode(loopVar.lexeme, listExpression, body);
}

StatementNode* foreign_declaration()
{
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
    expect(tLET);
    Token constructor = expect(tUIDENT);
    ParamListNode* params = parameter_list();
    expect('=');
    ExpressionNode* body = expression();
    expect(tEOL);

    return new MatchNode(constructor.lexeme, params, body);
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
    MemberList* memberList = member_list();

    return new StructDefNode(name.lexeme, memberList);
}

StatementNode* while_statement()
{
    expect(tWHILE);
    ExpressionNode* condition = expression();
    expect(tDO);
    StatementNode* body = suite();

    return new WhileNode(condition, body);
}

StatementNode* assignment_node()
{
    AssignableNode* lhs = assignable_node();

    if (accept('='))
    {
        ExpressionNode* rhs = expression();
        return new AssignNode(lhs, rhs);
    }

    ArgList* argList = new ArgList;
    argList->emplace_back(lhs);

    std::string functionName;
    switch (peekType())
    {
    case tPLUS_EQUAL:
        functionName = '+';
        break;

    case tMINUS_EQUAL:
        functionName = '-';

    case tTIMES_EQUAL:
        functionName = '*';
        break;

    case tDIV_EQUAL:
        functionName = '/';
        break;

    default:
        std::cerr << "ERROR: Unexpected " << nextTokens[0].type << std::endl;
        exit(1);

    }

    ExpressionNode* rhs = expression();
    argList->emplace_back(rhs);

    expect(tEOL);

    return new AssignNode($1->clone(), new FunctionCallNode(functionName, argList));
}

AssignableNode* assignable()
{
    Token token = expect(tLIDENT);
    if (peekType() == '{')
    {
        expect('{');
        Token memberName = expect(tLIDENT);
        expect('}');

        return new MemberAccessNode(token.lexeme, memberName.lexeme);
    }

    return new VariableNode(token.lexeme);
}

StatementNode* variable_declaration()
{
    if (peekType() == tLIDENT)
    {
        Token varName = expect(tLIDENT);
        TypeDecl* varType = optional_type();
        expect(tCOLON_EQUAL);
        ExpressionNode* value = expression();
        expect(tEOL);

        return new LetNode(varName.lexeme, varType, value);
    }
    else
    {
        expect(tVAR);
        Token varName = expect(tLIDENT);
        TypeDecl* varType = optional_type();
        expect('=');
        ExpressionNode* value = expression();
        expect(tEOL);

        return new LetNode(varName.lexeme, varType, value);
    }
}

StatementNode* while_statement()
{
    expect(tWHILE);
    ExpressionNode* condition = expression();
    expect(tDO);
    StatementNode* body = suite();

    return new WhileNode(condition, body);
}

//// Types /////////////////////////////////////////////////////////////////////

TypeDecl* optional_type()
{
    if (accept(tDCOLON))
    {
        return type_declaration();
    }
    else
    {
        return nullptr;
    }
}

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
        Token typeName = expect(tUIDENT);
        return new TypeName(typeName.lexeme);
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


