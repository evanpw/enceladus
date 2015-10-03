/*
Format:
|   alternation
()  grouping
[]  option (0 or 1 times)
{}  repetition (0 to n times)
*/

program
    : { statement }

//// Statements ////////////////////////////////////////////////////////////////

statement
    : EOL
    | if_statement
    | assert_statement
    | data_declaration
    | type_alias_declaration
    | function_definition
    | for_statement
    | foreign_declaration
    | forever_statement
    | let_statement
    | match_statement
    | return_statement
    | struct_declaration
    | variable_declaration
    | while_statement
    | break_statement
    | pass_statement
    | implementation_block
    | assign_or_expr

if_statement
    : IF expression suite { ELIF expression suite } [ ELSE suite ]

assert_statement
    : ASSERT expression EOL

pass_statement
    : PASS expression EOL

assign_or_expr
    : expression '=' expression EOL
    | expression DIV_EQUAL expression EOL
    | expression MINUS_EQUAL expression EOL
    | expression PLUS_EQUAL expression EOL
    | expression TIMES_EQUAL expression EOL
    | expression EOL

data_declaration
    : DATA UIDENT type_params '=' constructor_spec { '|' constructor_spec } EOL

type_alias_declaration
    : TYPE UIDENT '=' type EOL

function_definition
    : DEF ident type_params params_and_types suite

foreign_declaration
    : FOREIGN ident type_params params_and_types EOL

for_statement
    : FOR LIDENT IN expression suite
    | FOR LIDENT '=' expression TO expression suite

forever_statement
    : FOREVER suite

let_statement
    : LET UIDENT parameters COLON_EQUAL expression EOL

match_statement
    : MATCH expression EOL match_body

match_body
    : INDENT match_arm { match_arm } DEDENT

match_arm
    : UIDENT parameters ( '=>' statement | EOL INDENT statement_list DEDENT)

return_statement
    : RETURN expression EOL

struct_declaration
    : STRUCT UIDENT type_params '=' members

variable_declaration
    : LIDENT [ ':' type ] COLON_EQUAL expression EOL
    | VAR LIDENT [ ':' type ] '=' expression EOL

while_statement
    : WHILE expression suite

break_statement
    : BREAK EOL

implementation_block
    : IMPL type_params type EOL INDENT method_definition { method_definition } DEDENT

method_definition
    : DEF ident type_params params_and_types suite


//// Types /////////////////////////////////////////////////////////////////////

type
    : '|' [ arrow_type { ',' arrow_type } ] '|' RARROW constructed_type
    | arrow_type

arrow_type
    : constructed_type [ RARROW constructed_type ]

constructed_type
    : UIDENT [ '<' type { ',' type } '>' ]
    | simple_type

simple_type
    : LIDENT
    | UIDENT
    | '[' type ']'
    | '(' type ')'

//// Miscellaneous /////////////////////////////////////////////////////////////

constructor_spec
    : UIDENT [ '(' type { ',' type } ')' ]

suite
    : ':' statement
    | EOL INDENT statement_list DEDENT

statement_list
    : statement
    | statement_list statement

parameters
    : '(' LIDENT { ',' LIDENT } ')'
    | /* empty */

param_and_type
    : LIDENT ':' type

params_and_types
    : '(' [ param_and_type { ',' param_and_type } ] ')' RARROW constructed_type

type_params
    : '<' UIDENT { ',' UIDENT } '>'
    | /* empty */

 //// Structures ///////////////////////////////////////////////////////////////

members
    : EOL INDENT member_definition { member_definition } DEDENT

member_definition
    : LIDENT ':' type EOL

 //// Expressions //////////////////////////////////////////////////////////////

expression
    : and_expression OR expression
    | and_expression

and_expression
    : equality_expression AND and_expression
    | equality_expression

equality_expression
    : relational_expression [ ( EQUALS | NE ) relational_expression ]

relational_expression
    : cons_expression [ ( '>' | '<' | GE | LE ) cons_expression ]

cons_expression
    : additive_expression [ DCOLON cons_expression ]

additive_expression
    : multiplicative_expression { ( '+' | '-' ) multiplicative_expression }

multiplicative_expression
    : concat_expression { ( '*' | '/' | MOD ) concat_expression }

concat_expression
    : negation_expression [ '++' concat_expression ]

negation_expression
    : method_member_idx_expression
    | '-' method_member_idx_expression
    | NOT method_member_idx_expression

method_member_idx_expression
    : func_call_expression
    | method_member_idx_expression '.' LIDENT
    | method_member_idx_expression '.' LIDENT '(' [ expression ] { ',' expression } ] ')'
    | method_member_idx_expression '[' expression ']'

func_call_expression
    : ident '$' expression
    | ident '(' [ expression ] { ',' expression } ] ')'
    | unary_expression

unary_expression
    | '(' expression ')'
    | ident
    | TRUE
    | FALSE
    | inline_list
    | INT_LIT
    | UINT_LIT
    | STRING_LIT

inline_list
    : '[' list_interior ']'
    | '[' ']'

list_interior
    : expression { ',' expression }

ident
    : LIDENT
    | UIDENT
