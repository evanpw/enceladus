%{
#include <iostream>
#include "ast.hpp"
#include "memory_manager.hpp"
#include "string_table.hpp"

using namespace std;

//#define YYDEBUG 1

extern int yylex();
extern int line_number;
extern ProgramNode* root;
void yyerror(const char* msg);

%}

%define "parse.lac" "full"
%error-verbose
%locations

%union
{
	ProgramNode* program;
	AstNode* line;
	LabelNode* label;
	StatementNode* statement;
	VariableNode* variable;
	ExpressionNode* expression;
	const char* str;
}

%type<program> program
%type<line> line
%type<label> label
%type<statement> statement
%type<expression> expression
%type<variable> variable

%token ERROR IF THEN GOTO PRINT READ ASSIGN NOT EOL
%token<str> INT_LIT IDENT

%nonassoc NOT
%nonassoc '>' '='
%left '+' '-'
%left '*' '/'

%%

program: /* empty */
		{
			root = $$ = ProgramNode::create();
		}
	| program line EOL
		{
			// Ignore blank lines
			if ($2 != NULL)
				$1->prepend($2);
			$$ = $1;
		}
	| program error EOL // An error on one line shouldn't break all other lines
		{
			$$ = $1;
		}
	;

line: /* empty */
		{
			$$ = NULL;
		}
	| label
		{
			$$ = $1;
		}
	| statement
		{
			$$ = $1;
		}
	;
     
label: IDENT ':'
		{
			$$ = LabelNode::create($1);
		}
	;

statement: IF expression THEN statement
		{
			$$ = IfNode::create($2, $4);
		}
	| GOTO IDENT
		{
			$$ = GotoNode::create($2);
		}
	| PRINT expression
		{
			$$ = PrintNode::create($2);
		}
	| READ variable
		{
			$$ = ReadNode::create($2);
		}
	| variable ASSIGN expression
		{
			$$ = AssignNode::create($1, $3);
		}
	;
	   
expression: NOT expression
		{
			$$ = NotNode::create($2);
		}
	| expression '>' expression
		{
			$$ = GreaterNode::create($1, $3);
		}
	| expression '=' expression
		{
			$$ = EqualNode::create($1, $3);
		}
	| expression '+' expression
		{
			$$ = PlusNode::create($1, $3);
		}
	| expression '-' expression
		{
			$$ = MinusNode::create($1, $3);
		}
	| expression '*' expression
		{
			$$ = TimesNode::create($1, $3);
		}
	| expression '/' expression
		{
			$$ = DivideNode::create($1, $3);
		}
	| '(' expression ')'
		{
			$$ = $2;
		}
	| variable
		{
			$$ = static_cast<ExpressionNode*>($1);
		}
	| INT_LIT
		{
			$$ = IntNode::create($1);
		}
	;
	
variable: IDENT
	{
		$$ = VariableNode::create($1);
	}

%%

void yyerror(const char* msg)
{
	std::cerr << "Near line " << yylloc.first_line << ", "
			  << "column " << yylloc.first_column << ": "
			  << msg << std::endl;
}
