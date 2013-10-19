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
	AstNode* line;
	LabelNode* label;
	StatementNode* statement;
	BlockNode* block;
	ExpressionNode* expression;
	ParamListNode* params;
	ArgList* arguments;
	const char* str;
	long number;
}

%type<program> program
%type<line> logical_line
%type<label> label
%type<statement> statement suite
%type<expression> expression
%type<block> statement_list
%type<params> param_list
%type<arguments> arg_list;

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
			root = $$ = new ProgramNode;
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
			$$ = new LabelNode($1);
		}
	;

statement: IF expression THEN suite
		{
			$$ = new IfNode($2, $4);
		}
	| IF expression THEN suite ELSE suite
		{
			$$ = new IfElseNode($2, $4, $6);
		}
	| GOTO IDENT EOL
		{
			$$ = new GotoNode($2);
		}
	| PRINT expression EOL
		{
			$$ = new PrintNode($2);
		}
	| WHILE expression DO suite
		{
			$$ = new WhileNode($2, $4);
		}
	| IDENT ASSIGN expression EOL
		{
			$$ = new AssignNode($1, $3);
		}
	| DEF IDENT '(' ')' AS suite
		{
			$$ = new FunctionDefNode($2, $6, new ParamListNode());
		}
	| DEF IDENT '(' param_list ')' AS suite
		{
			$$ = new FunctionDefNode($2, $7, $4);
		}
	| RETURN expression EOL
		{
			$$ = new ReturnNode($2);
		}
	;

param_list: IDENT
		{
			$$ = new ParamListNode();
			$$->prepend($1);
		}
	| param_list ',' IDENT
		{
			$1->prepend($3);
			$$ = $1;
		}
	;

arg_list: expression
		{
			$$ = new ArgList();
			$$->emplace_back($1);
		}
	| arg_list ',' expression
		{
			$1->emplace_back($3);
			$$ = $1;
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
			$$ = new BlockNode;
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
	| CALL IDENT '(' ')'
		{
			$$ = new FunctionCallNode($2, new ArgList());
		}
	| CALL IDENT '(' arg_list ')'
		{
			$$ = new FunctionCallNode($2, $4);
		}
	| READ
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
	;

%%

void yyerror(const char* msg)
{
	std::cerr << "Near line " << yylloc.first_line << ", "
			  << "column " << yylloc.first_column << ": "
			  << msg << std::endl;
}
