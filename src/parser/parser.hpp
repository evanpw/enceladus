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
    ProgramNode* program();
    StatementNode* statement();
    PassNode* pass_statement();
    StatementNode* if_statement();
    StatementNode* if_helper(const YYLTYPE& location);
    AssertNode* assert_statement();
    DataDeclaration* data_declaration();
    TypeAliasNode* type_alias_declaration();
    FunctionDefNode* function_definition();
    ForNode* for_statement();
    ForeignDeclNode* foreign_declaration();
    ForeverNode* forever_statement();
    LetNode* let_statement();
    MatchNode* match_statement();
    MatchArm* match_arm();
    ReturnNode* return_statement();
    StructDefNode* struct_declaration();
    WhileNode* while_statement();
    StatementNode* assign_or_expr();
    VariableDefNode* variable_declaration();
    BreakNode* break_statement();
    ImplNode* implementation_block();
    MethodDefNode* method_definition();
    TraitDefNode* trait_definition();
    TraitMethodNode* trait_method();

    StatementNode* suite();
    std::vector<std::string> parameters();
    std::string ident();

    TypeName* type();
    TypeName* arrow_type();
    TypeName* constructed_type();
    TypeName* simple_type();
    TypeName* trait_name();
    ConstructorSpec* constructor_spec();
    std::pair<std::string, TypeName*> param_and_type();
    std::pair<std::vector<std::string>, TypeName*> params_and_types();
    std::vector<std::string> type_params();
    std::vector<TypeParam> constrained_type_params();
    std::vector<TypeParam> where_clause();
    TypeParam constrained_type_param();


    std::vector<MemberDefNode*> members();
    MemberDefNode* member_definition();

    ExpressionNode* expression();
    ExpressionNode* and_expression();
    ExpressionNode* equality_expression();
    ExpressionNode* relational_expression();
    ExpressionNode* additive_expression();
    ExpressionNode* multiplicative_expression();
    ExpressionNode* negation_expression();
    ExpressionNode* cast_expression();
    ExpressionNode* method_member_idx_expression();
    ExpressionNode* func_call_expression();
    ExpressionNode* static_function_call_expression();
    ExpressionNode* unary_expression();
    ExpressionNode* integer_literal();
};

#endif
