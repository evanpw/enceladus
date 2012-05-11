%{
#include <iostream>

using namespace std;

#define YYSTYPE const char*

extern int yylex();

void yyerror(const char* msg)
{
	std::cerr << "Parse error: " << msg << std::endl; 
}

%}

%token ERROR
%token IF
%token THEN
%token GOTO
%token PRINT
%token READ
%token ASSIGN
%token NOT
%token INT_LIT
%token IDENT
%token EOL

%%

line_list:
	  | line_list line EOL
	  ;

line:
     | IDENT ':'
     | statement

statement: IF expression THEN statement
	   | GOTO IDENT
	   | PRINT IDENT
	   | READ IDENT
	   | IDENT ASSIGN expression
	   ;

expression: conditional_expression
	    | NOT conditional_expression
	    ;

conditional_expression:	additive_expression
			| conditional_expression '>' additive_expression
			| conditional_expression '=' additive_expression
			;

additive_expression: multiplicative_expression
		     | additive_expression '+' multiplicative_expression
		     | additive_expression '-' multiplicative_expression
		     ;

multiplicative_expression: primary_expression
			   | multiplicative_expression '*' primary_expression
			   | multiplicative_expression '/' primary_expression
			   ;

primary_expression: '(' expression ')'
	            | IDENT
	            | INT_LIT
	            ;

%%

int main()
{
	yyparse();
}
