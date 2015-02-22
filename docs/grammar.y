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
    | assignment_statement
    | data_declaration
    | type_alias_declaration
    | function_definition
    | for_statement
    | foreign_declaration
    | match_statement
    | return_statement
    | struct_declaration
    | variable_declaration
    | while_statement
    | break_statement
    | expression EOL

if_statement
    : IF expression THEN suite
    | IF expression THEN suite ELSE suite

assignment_statement
    : LIDENT '=' expression EOL
    | LIDENT DIV_EQUAL expression EOL
    | LIDENT MINUS_EQUAL expression EOL
    | LIDENT PLUS_EQUAL expression EOL
    | LIDENT TIMES_EQUAL expression EOL

data_declaration
    : DATA UIDENT { LIDENT } '=' constructor_spec EOL

type_alias_declaration
    : TYPE UIDENT '=' type EOL

function_definition
    : DEF ident params_and_types '=' suite

foreign_declaration
    : FOREIGN ident params_and_types EOL

for_statement
    : FOR LIDENT IN expression DO suite

match_statement
    : LET UIDENT parameters '=' expression EOL

return_statement
    : RETURN expression EOL

struct_declaration
    : STRUCT UIDENT '=' members

variable_declaration
    : LIDENT [ ':' type ] COLON_EQUAL expression EOL
    | VAR LIDENT [ ':' type ] '=' expression EOL

while_statement
    : WHILE expression DO suite

break_statement
    : BREAK EOL


//// Types /////////////////////////////////////////////////////////////////////

type
    : '|' [ arrow_type { ',' arrow_type } ] '|' RARROW constructed_type
    | arrow_type

arrow_type
    : constructed_type [ RARROW constructed_type ]

constructed_type
    : UIDENT { simple_type }
    | simple_type

simple_type
    : LIDENT
    | UIDENT
    | '[' type ']'
    | '(' type ')'

//// Miscellaneous /////////////////////////////////////////////////////////////

constructor_spec
    : UIDENT { simple_type }

suite
    : statement
    | EOL INDENT statement_list DEDENT

statement_list
    : statement
    | statement_list statement

parameters
    : { LIDENT }

param_and_type
    : LIDENT ':' type

params_and_types
    : '(' [ param_and_type { ',' param_and_type } ] ')' RARROW constructed_type

 //// Structures ///////////////////////////////////////////////////////////////

members
    : member_definition
    | EOL INDENT member_list DEDENT

member_definition
    : LIDENT ':' type EOL

member_list
    : member_definition
    | member_list member_definition

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
    : additive_expression [ ':' cons_expression ]

additive_expression
    : multiplicative_expression { ( '+' | '-' ) multiplicative_expression }

multiplicative_expression
    : concat_expression { ( '*' | '/' | MOD ) concat_expression }

concat_expression
    : negation_expression [ CONCAT concat_expression ]

negation_expression
    : [ '-' ] func_call_expression

func_call_expression
    : ident { unary_expression } [ '$' expression ]
    | unary_expression

unary_expression
    | '(' expression ')'
    | ident
    | LIDENT '{' LIDENT '}'
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

