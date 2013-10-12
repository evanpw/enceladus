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
	AstNode* line;
	LabelNode* label;
	StatementNode* statement;
	BlockNode* block;
	ExpressionNode* expression;
	const char* str;
	long number;
}

%type<program> program
%type<line> logical_line
%type<label> label
%type<statement> statement suite
%type<expression> expression
%type<block> statement_list

%token ERROR IF THEN ELSE GOTO PRINT READ ASSIGN NOT RETURN
%token AND OR EOL MOD WHILE DO INDENT DEDENT EQUALS DEF AS CALL
%token<str> IDENT
%token<number> INT_LIT WHITESPACE

%nonassoc NOT
%left AND OR
%nonassoc '>' '<' LE GE EQUALS NE
%left '+' '-'
%left '*' '/' MOD

%%

program: /* empty */
		{
			root = $$ = ProgramNode::create();
		}
	| program logical_line
		{
			// Ignore blank lines
			if ($2 != NULL)
				$1->prepend($2);

			$$ = $1;
		}
	;

logical_line: label
		{
			$$ = $1;
		}
	| statement
		{
			$$ = $1;
		}
	;

label: IDENT ':' EOL
		{
			$$ = LabelNode::create($1);
		}
	;

statement: IF expression THEN suite
		{
			$$ = IfNode::create($2, $4);
		}
	| IF expression THEN suite ELSE suite
		{
			$$ = IfElseNode::create($2, $4, $6);
		}
	| GOTO IDENT EOL
		{
			$$ = GotoNode::create($2);
		}
	| PRINT expression EOL
		{
			$$ = PrintNode::create($2);
		}
	| WHILE expression DO suite
		{
			$$ = WhileNode::create($2, $4);
		}
	| IDENT ASSIGN expression EOL
		{
			$$ = AssignNode::create($1, $3);
		}
	| DEF IDENT AS suite
		{
			$$ = FunctionDefNode::create($2, $4);
		}
	| RETURN expression EOL
		{
			$$ = ReturnNode::create($2);
		}
	;

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
			$$ = BlockNode::create();
			$$->prepend($1);
		}
	| statement_list statement
		{
			$1->prepend($2);
			$$ = $1;
		}
	;

expression: NOT expression
		{
			$$ = NotNode::create($2);
		}
	| expression AND expression
		{
			$$ = LogicalNode::create($1, LogicalNode::kAnd, $3);
		}
	| expression OR expression
		{
			$$ = LogicalNode::create($1, LogicalNode::kOr, $3);
		}
	| expression '>' expression
		{
			$$ = ComparisonNode::create($1, ComparisonNode::kGreater, $3);
		}
	| expression '<' expression
		{
			$$ = ComparisonNode::create($1, ComparisonNode::kLess, $3);
		}
	| expression GE expression
		{
			$$ = ComparisonNode::create($1, ComparisonNode::kGreaterOrEqual, $3);
		}
	| expression LE expression
		{
			$$ = ComparisonNode::create($1, ComparisonNode::kLessOrEqual, $3);
		}
	| expression EQUALS expression
		{
			$$ = ComparisonNode::create($1, ComparisonNode::kEqual, $3);
		}
	| expression NE expression
		{
			$$ = ComparisonNode::create($1, ComparisonNode::kNotEqual, $3);
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
	| expression MOD expression
		{
			$$ = BinaryOperatorNode::create($1, BinaryOperatorNode::kMod, $3);
		}
	| CALL IDENT
		{
			$$ = FunctionCallNode::create($2);
		}
	| READ
		{
			$$ = ReadNode::create();
		}
	| '(' expression ')'
		{
			$$ = $2;
		}
	| IDENT
		{
			$$ = VariableNode::create($1);
		}
	| INT_LIT
		{
			$$ = IntNode::create($1);
		}
	;

%%

void yyerror(const char* msg)
{
	std::cerr << "Near line " << yylloc.first_line << ", "
			  << "column " << yylloc.first_column << ": "
			  << msg << std::endl;
}
