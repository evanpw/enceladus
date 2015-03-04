#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"

ProgramNode* parse();

ProgramNode* program();
StatementNode* statement();
StatementNode* if_statement();
StatementNode* data_declaration();
StatementNode* type_alias_declaration();
StatementNode* function_definition();
StatementNode* for_statement();
StatementNode* foreign_declaration();
StatementNode* forever_statement();
StatementNode* match_statement();
StatementNode* return_statement();
StatementNode* struct_declaration();
StatementNode* while_statement();
StatementNode* assignment_statement();
StatementNode* variable_declaration();
StatementNode* while_statement();
StatementNode* break_statement();
StatementNode* suite();
ParamList* parameters();
std::pair<std::string, TypeName*> param_and_type();
std::pair<ParamList*, TypeName*> params_and_types();
TypeName* type();
TypeName* arrow_type();
TypeName* constructed_type();
TypeName* simple_type();
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
ExpressionNode* negation_expression();
ExpressionNode* func_call_expression();
ExpressionNode* unary_expression();
std::string ident();

#endif
