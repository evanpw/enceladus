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
%expect 1 // if-then-else statements

%union
{
	ProgramNode* program;
	BlockNode* block;
	AstNode* line;
	LabelNode* label;
	StatementNode* statement;
	VariableNode* variable;
	ExpressionNode* expression;
	const char* str;
	int number;
}

%type<program> program
%type<line> line
%type<label> label
%type<statement> statement suite
%type<expression> expression
%type<variable> variable
%type<block> block statement_list

%token ERROR IF THEN ELSE GOTO PRINT READ ASSIGN NOT EOL
%token<str> IDENT
%token<number> INT_LIT

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

statement: block
		{
			$$ = $1;
		}
	| IF expression THEN suite
		{
			$$ = IfNode::create($2, $4);
		}
	| IF expression THEN suite ELSE suite
		{
			$$ = IfElseNode::create($2, $4, $6);
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
	
suite: EOL statement
		{
			$$ = $2;
		}
	| statement
		{
			$$ = $1;
		}
	
block: '{' EOL statement_list '}'
		{
			$$ = $3;
		}
	;
	
statement_list: 
		{
			$$ = BlockNode::create();
		}
	| statement_list statement EOL
		{
			$1->prepend($2);	
			$$ = $1;
		}
	;
	   
expression: NOT expression
		{
			$$ = NotNode::create($2);
		}
	| expression '>' expression
		{
			$$ = ComparisonNode::create($1, ComparisonNode::kGreater, $3);
		}
	| expression '=' expression
		{
			$$ = ComparisonNode::create($1, ComparisonNode::kEqual, $3);
		}
	| expression '+' expression
		{
			$$ = BinaryOperatorNode::create($1, BinaryOperatorNode::kPlus, $3);
		}
	| expression '-' expression
		{
			$$ = BinaryOperatorNode::create($1, BinaryOperatorNode::kMinus, $3);
		}
	| expression '*' expression
		{
			$$ = BinaryOperatorNode::create($1, BinaryOperatorNode::kTimes, $3);
		}
	| expression '/' expression
		{
			$$ = BinaryOperatorNode::create($1, BinaryOperatorNode::kDivide, $3);
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
