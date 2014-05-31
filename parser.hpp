#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"

////////////////////////////////////////////////////////////////////////////////

struct YYLTYPE
{
    int first_line;
    int first_column;
    int last_line;
    int last_column;
};

extern YYLTYPE yylloc;

union YYSTYPE
{
    const char* str;
    long number;
};

extern YYSTYPE yylval;

////////////////////////////////////////////////////////////////////////////////

ProgramNode* parse();

ProgramNode* program();
StatementNode* statement();
StatementNode* if_statement();
StatementNode* data_declaration();
StatementNode* function_definition();
StatementNode* for_statement();
StatementNode* foreign_declaration();
StatementNode* match_statement();
StatementNode* return_statement();
StatementNode* struct_declaration();
StatementNode* while_statement();
StatementNode* assignment_statement();
StatementNode* variable_declaration();
StatementNode* while_statement();
AssignableNode* assignable();
StatementNode* suite();
ParamListNode* parameters();
TypeDecl* type_declaration();
TypeName* type();
ConstructorSpec* constructor_spec();
MemberList* members();
MemberDefNode* member_definition();
ExpressionNode* expression();
ExpressionNode* and_expression();
ExpressionNode* equality_expression();
ExpressionNode* relational_expression();
ExpressionNode* cons_expression();
ExpressionNode* additive_expression();
ExpressionNode* multiplicative_expression();
ExpressionNode* concat_expression();
ExpressionNode* func_call_expression();
ExpressionNode* unary_expression();
std::string ident();

#endif
