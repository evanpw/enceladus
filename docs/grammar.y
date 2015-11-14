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
    | type_alias
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
    | continue_statement
    | pass_statement
    | implementation_block
    | assign_or_expr

if_statement
    : IF ( expression | let_expression ) { ELIF ( expression | let_expression ) } [ ELSE suite ]

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
    | expression REM_EQUAL expression EOL
    | expression EOL

data_declaration
    : DATA UIDENT type_params '=' constructor_spec { '|' constructor_spec } EOL

type_alias
    : TYPE UIDENT '=' type EOL

function_definition
    : DEF ident params_and_types [ where_clause ] suite

foreign_declaration
    : FOREIGN ident type_params params_and_types EOL

for_statement
    : FOR LIDENT IN expression suite

forever_statement
    : FOREVER suite

let_statement
    : LET UIDENT parameters COLON_EQUAL expression EOL

let_expression
    : LET UIDENT parameters COLON_EQUAL expression

let_statement
    : let_expression EOL

match_statement
    : MATCH expression EOL match_body

match_body
    : INDENT match_arm { match_arm } DEDENT

match_arm
    : UIDENT parameters ( '=>' statement | EOL INDENT statement_list DEDENT)

return_statement
    : RETURN [ expression ] EOL

struct_declaration
    : STRUCT UIDENT constrained_type_params [ where_clause ] members

variable_declaration
    : LIDENT COLON_EQUAL expression EOL

while_statement
    : WHILE ( expression | let_expression ) suite

break_statement
    : BREAK EOL

continue_statement
    : CONTINUE EOL

implementation_block
    : IMPL type [ FOR type ] [ ':' UIDENT { '+' UIDENT } ] where_clause EOL INDENT [ member { member } DEDENT ]

member
    : method_definition
    | type_alias

method_definition
    : DEF ident params_and_types where_clause suite

trait_definition
    : TRAIT UIDENT type_params EOL [ INDENT trait_member { trait_member } DEDENT ]

trait_member
    : trait_method
    | associated_type

trait_method
    : DEF LIDENT params_and_types EOL

associated_type
    TYPE constrained_type_param EOL


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
    : UIDENT
    | '[' type ']'

trait_name
    : UIDENT [ '<' type {',' type } '>' ]

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
    : '(' [ param_and_type { ',' param_and_type } ] ')' [ RARROW constructed_type ]

method_params_and_types
    : '(' [ LIDENT [ ': type ] { ',' param_and_type } ] ')' [ RARROW constructed_type ]

type_params
    : '<' UIDENT { ',' UIDENT } '>'
    | /* empty */

constrained_type_params
    : '<' constrained_type_param { ',' constrained_type_param } '>'
    | /* empty */

constrained_type_param
    : UIDENT [':' UIDENT { '+' UIDENT }]

where_clause
    : WHERE constrained_type_param { ',' constrained_type_param }
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
    : cons_expression [ ( '>' | '<' | GE | LE ) additive_expression ]

additive_expression
    : multiplicative_expression { ( '+' | '-' ) multiplicative_expression }

multiplicative_expression
    : negation_expression { ( '*' | '/' | '%' ) negation_expression }

negation_expression
    : cast_expression
    | '-' cast_expression

cast_expression
    : method_member_idx_expression AS type
    | method_member_idx_expression

method_member_idx_expression
    : func_call_expression
    | method_member_idx_expression '.' LIDENT
    | method_member_idx_expression '.' LIDENT '(' [ expression ] { ',' expression } ] ')'
    | method_member_idx_expression '[' expression ']'

func_call_expression
    : ident '$' expression
    | ident '(' [ expression { ',' expression } ] ')'
    | static_function_call_expression

static_function_call_expression
    : type '::' LIDENT '(' [ expression { ',' expression } ] ')'
    | type '::' LIDENT '$' expression
    | unary_expression

unary_expression
    | '(' expression ')'
    | ident
    | TRUE
    | FALSE
    | inline_list
    | INT_LIT
    | STRING_LIT

inline_list
    : '[' list_interior ']'
    | '[' ']'

list_interior
    : expression { ',' expression }

ident
    : LIDENT
    | UIDENT
