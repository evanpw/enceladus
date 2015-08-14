#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast/ast.hpp"
#include "ast/ast_context.hpp"

class Parser
{
public:
    Parser(AstContext* context)
    : _context(context)
    {}

    void parse();
    AstContext* context() { return _context; }

private:
    AstContext* _context;

private:
    // Individual grammar constructions
    ProgramNode* program();
    StatementNode* statement();
    StatementNode* if_statement();
    StatementNode* if_helper(const YYLTYPE& location);
    StatementNode* data_declaration();
    StatementNode* type_alias_declaration();
    StatementNode* function_definition();
    StatementNode* for_statement();
    StatementNode* foreign_declaration();
    StatementNode* forever_statement();
    StatementNode* let_statement();
    StatementNode* match_statement();
    MatchArm* match_arm();
    StatementNode* return_statement();
    StatementNode* struct_declaration();
    StatementNode* while_statement();
    StatementNode* assignment_statement();
    StatementNode* variable_declaration();
    StatementNode* break_statement();
    StatementNode* suite();
    std::vector<std::string> parameters();
    std::pair<std::string, TypeName*> param_and_type();
    std::pair<std::vector<std::string>, TypeName*> params_and_types();
    TypeName* type();
    TypeName* arrow_type();
    TypeName* constructed_type();
    TypeName* simple_type();
    ConstructorSpec* constructor_spec();
    std::vector<MemberDefNode*> members();
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
};

#endif
