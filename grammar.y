program
    : /* empty */
    | program statement

//// Statements ////////////////////////////////////////////////////////////////

statement
    : if_statement
    | assignment_statement
    | data_declaration
    | function_definition
    | for_statement
    | foreign_declaration
    | match_statement
    | return_statement
    | struct_declaration
    | variable_declaration
    | while_statement
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

function_definition
    : DEF ident parameters optional_type '=' suite

for_statement
    : FOR LIDENT IN expression DO suite

foreign_declaration
    : FOREIGN ident parameters DCOLON type_declaration EOL

match_statement
    : LET UIDENT parameter_list '=' expression EOL

return_statement
    : RETURN expression EOL

struct_declaration
    : STRUCT UIDENT '=' member_list

variable_declaration
    : LIDENT optional_type COLON_EQUAL expression EOL
    | VAR LIDENT optional_type '=' expression EOL

while_statement
    : WHILE expression DO suite


//// Types /////////////////////////////////////////////////////////////////////

optional_type
    : /* empty */
    | DCOLON type_declaration

type_declaration
    : type
    | type_declaration RARROW type

type
    : UIDENT
    | '[' type ']'

////////////////////////////////////////////////////////////////////////////////
constructor_spec
    : UIDENT
    | constructor_spec type

parameters
    : /* empty */
    | parameter_list

parameter_list
    : LIDENT
    | parameter_list LIDENT

suite
    : statement
    | EOL INDENT statement_list DEDENT

statement_list
    : statement
    | statement_list statement

 //// Structures ///////////////////////////////////////////////////////////////

member_list
    : member_def
    | EOL INDENT member_list DEDENT

member_def
    : LIDENT DCOLON type EOL

member_list
    : member_def
    | member_list member_def

 //// Expressions //////////////////////////////////////////////////////////////

expression
    : expression AND expression
    | expression OR expression
    | expression '>' expression
    | expression '<' expression
    | expression GE expression
    | expression LE expression
    | expression EQUALS expression
    | expression NE expression
    | expression '+' expression
    | expression '-' expression
    | expression '*' expression
    | expression '/' expression
    | expression MOD expression
    | expression ':' expression
    | expression CONCAT expression
    | simple_expression
    | ident arg_list arg_list_tail
    | UIDENT '{' '}'

arg_list_tail
    : '$' expression
    | simple_expression

arg_list
    : /* empty */
    | arg_list simple_expression

simple_expression
    : '(' expression ')'
    | ident
    | LIDENT '{' LIDENT '}'
    | INT_LIT
    | TRUE
    | FALSE
    | inline_list
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

