/*
Format:
|   alternation
()  grouping
[]  option (0 or 1 times)
{}  repetition (0 to n times)
*/

program
    : /* empty */
    | program statement

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
    : assignable '=' expression EOL
    | assignable DIV_EQUAL expression EOL
    | assignable MINUS_EQUAL expression EOL
    | assignable PLUS_EQUAL expression EOL
    | assignable TIMES_EQUAL expression EOL

assignable
    : LIDENT
    | LIDENT '{' LIDENT '}'

data_declaration
    : DATA UIDENT '=' constructor_spec EOL

type_alias_declaration
    : TYPE UIDENT '=' type EOL

function_definition
    : DEF ident parameters [ DCOLON type_declaration ] '=' suite

for_statement
    : FOR LIDENT IN expression DO suite

foreign_declaration
    : FOREIGN ident parameters DCOLON type_declaration EOL

match_statement
    : LET UIDENT parameters '=' expression EOL

return_statement
    : RETURN expression EOL

struct_declaration
    : STRUCT UIDENT '=' members

variable_declaration
    : LIDENT [ type ] COLON_EQUAL expression EOL
    | VAR LIDENT [ type ] '=' expression EOL

while_statement
    : WHILE expression DO suite

break_statement
    : BREAK EOL


//// Types /////////////////////////////////////////////////////////////////////

type_declaration
    : type
    | type_declaration RARROW type

type
    : UIDENT
    | '[' type ']'

constructor_spec
    : UIDENT
    | constructor_spec type

suite
    : statement
    | EOL INDENT statement_list DEDENT

statement_list
    : statement
    | statement_list statement

parameters
    : { LIDENT }

 //// Structures ///////////////////////////////////////////////////////////////

members
    : member_definition
    | EOL INDENT member_list DEDENT

member_definition
    : LIDENT DCOLON type EOL

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
    : relational_expression EQUALS equality_expression
    | relational_expression NE equality_expression
    | relational_expression

relational_expression
    : cons_expression '>' relational_expression
    | cons_expression '<' relational_expression
    | cons_expression GE relational_expression
    | cons_expression LE relational_expression
    | cons_expression

cons_expression
    : additive_expression ':' cons_expression
    | additive_expression

additive_expression
    : multiplicative_expression '+' additive_expression
    | multiplicative_expression '-' additive_expression
    | multiplicative_expression

multiplicative_expression
    : concat_expression '*' multiplicative_expression
    | concat_expression '/' multiplicative_expression
    | concat_expression MOD multiplicative_expression
    | concat_expression

concat_expression
    : negation_expression CONCAT concat_expression
    | negation_expression

negation_expression
    : '-' expression
    | func_call_expression

func_call_expression
    : ident { unary_expression } [ '$' expression ]
    | unary_expression

unary_expression
    | '(' expression ')'
    | ident
    | UIDENT '{' '}'
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
    : expression
    | list_interior ',' expression

ident
    : LIDENT
    | UIDENT

