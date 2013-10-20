%{
#include <iostream>
#include "ast.hpp"
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
	StatementNode* statement;
	BlockNode* block;
	ExpressionNode* expression;
	ParamListNode* params;
	ArgList* arguments;
	const char* str;
	long number;
}

%type<program> program
%type<statement> statement suite
%type<expression> expression fexpression simple_expression
%type<block> statement_list
%type<params> param_list parameters
%type<arguments> arg_list

%token ERROR
%token IF THEN ELSE
%token PRINT READ
%token NOT AND OR MOD EQUALS
%token RETURN
%token WHILE DO
%token INDENT DEDENT
%token EOL
%token DEF
%token DCOLON
%token TRUE FALSE
%token PLUS_EQUAL MINUS_EQUAL TIMES_EQUAL DIV_EQUAL
%token<str> IDENT
%token<number> INT_LIT
%token<number> WHITESPACE // Handled by the second stage of the lexer - won't be seen by parser

%nonassoc NOT
%left AND OR
%nonassoc '>' '<' LE GE EQUALS NE
%left '+' '-'
%left '*' '/' MOD

%%

program: /* empty */
		{
			root = $$ = new ProgramNode;
		}
	| program statement
		{
			$1->append($2);
			$$ = $1;
		}

statement: IF expression THEN suite
		{
			$$ = new IfNode($2, $4);
		}
	| IF expression THEN suite ELSE suite
		{
			$$ = new IfElseNode($2, $4, $6);
		}
	| PRINT expression EOL
		{
			$$ = new PrintNode($2);
		}
	| WHILE expression DO suite
		{
			$$ = new WhileNode($2, $4);
		}
	| IDENT '=' expression EOL
		{
			$$ = new AssignNode($1, $3);
		}
	| IDENT PLUS_EQUAL expression EOL
		{
			$$ = new AssignNode($1, new BinaryOperatorNode(new VariableNode($1), BinaryOperatorNode::kPlus, $3));
		}
	| IDENT MINUS_EQUAL expression EOL
		{
			$$ = new AssignNode($1, new BinaryOperatorNode(new VariableNode($1), BinaryOperatorNode::kMinus, $3));
		}
	| IDENT TIMES_EQUAL expression EOL
		{
			$$ = new AssignNode($1, new BinaryOperatorNode(new VariableNode($1), BinaryOperatorNode::kTimes, $3));
		}
	| IDENT DIV_EQUAL expression EOL
		{
			$$ = new AssignNode($1, new BinaryOperatorNode(new VariableNode($1), BinaryOperatorNode::kDivide, $3));
		}
	| DEF IDENT parameters DCOLON IDENT '=' suite
		{
			$$ = new FunctionDefNode($2, $7, $3, $5);
		}
	| RETURN expression EOL
		{
			$$ = new ReturnNode($2);
		}

parameters: /* empty */
		{
			$$ = new ParamListNode();
		}
	| param_list
		{
			$$ = $1;
		}

param_list: IDENT
		{
			$$ = new ParamListNode();
			$$->append($1);
		}
	| param_list IDENT
		{
			$1->append($2);
			$$ = $1;
		}

suite: statement
		{
			$$ = $1;
		}
	| EOL INDENT statement_list DEDENT
		{
			$$ = $3;
		}

statement_list: statement
		{
			$$ = new BlockNode;
			$$->append($1);
		}
	| statement_list statement
		{
			$1->append($2);
			$$ = $1;
		}

/* An expression that is not a function call */
expression: NOT expression
		{
			$$ = new NotNode($2);
		}
	| expression AND expression
		{
			$$ = new LogicalNode($1, LogicalNode::kAnd, $3);
		}
	| expression OR expression
		{
			$$ = new LogicalNode($1, LogicalNode::kOr, $3);
		}
	| expression '>' expression
		{
			$$ = new ComparisonNode($1, ComparisonNode::kGreater, $3);
		}
	| expression '<' expression
		{
			$$ = new ComparisonNode($1, ComparisonNode::kLess, $3);
		}
	| expression GE expression
		{
			$$ = new ComparisonNode($1, ComparisonNode::kGreaterOrEqual, $3);
		}
	| expression LE expression
		{
			$$ = new ComparisonNode($1, ComparisonNode::kLessOrEqual, $3);
		}
	| expression EQUALS expression
		{
			$$ = new ComparisonNode($1, ComparisonNode::kEqual, $3);
		}
	| expression NE expression
		{
			$$ = new ComparisonNode($1, ComparisonNode::kNotEqual, $3);
		}
	| expression '+' expression
		{
			$$ = new BinaryOperatorNode($1, BinaryOperatorNode::kPlus, $3);
		}
	| expression '-' expression
		{
			$$ = new BinaryOperatorNode($1, BinaryOperatorNode::kMinus, $3);
		}
	| expression '*' expression
		{
			$$ = new BinaryOperatorNode($1, BinaryOperatorNode::kTimes, $3);
		}
	| expression '/' expression
		{
			$$ = new BinaryOperatorNode($1, BinaryOperatorNode::kDivide, $3);
		}
	| expression MOD expression
		{
			$$ = new BinaryOperatorNode($1, BinaryOperatorNode::kMod, $3);
		}
	| fexpression
		{
			$$ = $1;
		}

fexpression: simple_expression
		{
			$$ = $1;
		}
	| IDENT arg_list
		{
			$$ = new FunctionCallNode($1, $2);
		}

arg_list: simple_expression
        {
            $$ = new ArgList();
            $$->emplace_back($1);
        }
	| arg_list simple_expression
        {
            $1->emplace_back($2);
            $$ = $1;
        }

simple_expression: READ
		{
			$$ = new ReadNode;
		}
	| '(' expression ')'
		{
			$$ = $2;
		}
	| IDENT
		{
			$$ = new VariableNode($1);
		}
	| INT_LIT
		{
			$$ = new IntNode($1);
		}
	| TRUE
		{
			$$ = new BoolNode(true);
		}
	| FALSE
		{
			$$ = new BoolNode(false);
		}

%%

void yyerror(const char* msg)
{
	std::cerr << "Near line " << yylloc.first_line << ", "
			  << "column " << yylloc.first_column << ": "
			  << msg << std::endl;
}
