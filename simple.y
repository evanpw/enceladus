%{
#include <iostream>
#include "ast.hpp"
#include "memory_manager.hpp"
#include "string_table.hpp"

using namespace std;

//#define YYDEBUG 1

extern int yylex();

void yyerror(const char* msg)
{
	std::cerr << "Parse error: " << msg << std::endl; 
}

extern ProgramNode* root;

%}

%union
{
	ProgramNode* program;
	AstNode* line;
	LabelNode* label;
	StatementNode* statement;
	IdentNode* ident;
	ExpressionNode* expression;
	const char* str;
}

%type<program> program
%type<line> line
%type<label> label
%type<statement> statement
%type<expression> expression
%type<ident> ident

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
     
label: ident ':'
		{
			$$ = LabelNode::create($1);
		}
	;

statement: IF expression THEN statement
		{
			$$ = IfNode::create($2, $4);
		}
	| GOTO ident
		{
			$$ = GotoNode::create($2);
		}
	| PRINT expression
		{
			$$ = PrintNode::create($2);
		}
	| READ ident
		{
			$$ = ReadNode::create($2);
		}
	| ident ASSIGN expression
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
	| ident
	| INT_LIT
		{
			$$ = IntNode::create($1);
		}
	;
	
ident: IDENT
	{
		$$ = IdentNode::create($1);
	}

%%

ProgramNode* root;

int main()
{
	//yydebug = 1;
	yyparse();
	
	root->show(cout, 0);
	
	MemoryManager::freeNodes();
	return 0;
}
