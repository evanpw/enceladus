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
    StatementNode* pass_statement();
    StatementNode* if_statement();
    StatementNode* if_helper(const YYLTYPE& location);
    StatementNode* assert_statement();
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
    StatementNode* assign_or_expr();
    StatementNode* variable_declaration();
    StatementNode* break_statement();
    StatementNode* suite();
    StatementNode* implementation_block();
    StatementNode* method_definition();
    std::vector<std::string> parameters();
    std::pair<std::string, TypeName*> param_and_type();
    std::pair<std::vector<std::string>, TypeName*> params_and_types();
    std::vector<std::string> type_params();
    std::vector<std::pair<std::string, std::string>> constrained_type_params();
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
    ExpressionNode* cast_expression();
    ExpressionNode* method_member_idx_expression();
    ExpressionNode* func_call_expression();
    ExpressionNode* unary_expression();
    std::string ident();
    ExpressionNode* integer_literal();
};

#endif
