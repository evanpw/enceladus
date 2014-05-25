%{
#include <iostream>
#include <string>
#include "ast.hpp"

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
	AssignableNode* assignable;
	ExpressionNode* expression;
	ParamListNode* params;
	ArgList* arguments;
	TypeDecl* typeDecl;
	ConstructorSpec* constructorSpec;
	TypeName* typeName;
	MemberList* memberList;
	MemberDefNode* memberDef;
	const char* str;
	long number;
}

%type<program> program
%type<statement> statement suite
%type<assignable> assignable
%type<expression> expression simple_expression inline_list arg_list_tail
%type<block> statement_list
%type<params> param_list parameters
%type<arguments> arg_list list_interior
%type<typeDecl> typedecl optionaltype
%type<str> ident
%type<typeName> type
%type<constructorSpec> constructor_spec
%type<memberList> struct_def member_list;
%type<memberDef> member_def;

%token ERROR
%token IF THEN ELSE LET
%token DEF FOREIGN DATA COLON_EQUAL VAR STRUCT
%token AND OR MOD EQUALS
%token RETURN
%token WHILE DO
%token FOR IN
%token INDENT DEDENT
%token EOL
%token DCOLON RARROW
%token TRUE FALSE
%token PLUS_EQUAL MINUS_EQUAL TIMES_EQUAL DIV_EQUAL CONCAT
%token<str> STRING_LIT
%token<str> LIDENT UIDENT
%token<number> INT_LIT
%token<number> WHITESPACE // Handled by the second stage of the lexer - won't be seen by parser

%right '$'
%left AND OR
%nonassoc '>' '<' LE GE EQUALS NE
%right ':'
%left '+' '-'
%left '*' '/' MOD
%right CONCAT

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
	| WHILE expression DO suite
		{
			$$ = new WhileNode($2, $4);
		}
	| FOR LIDENT IN expression DO suite
		{
			$$ = makeForNode($2, $4, $6);
		}
	| assignable '=' expression EOL
		{
			$$ = new AssignNode($1, $3);
		}
	| LIDENT DCOLON type COLON_EQUAL expression EOL
		{
			$$ = new LetNode($1, $3, $5);
		}
	| LIDENT COLON_EQUAL expression EOL
		{
			$$ = new LetNode($1, nullptr, $3);
		}
	| VAR LIDENT DCOLON type '=' expression EOL
		{
			$$ = new LetNode($2, $4, $6);
		}
	| VAR LIDENT '=' expression EOL
		{
			$$ = new LetNode($2, nullptr, $4);
		}
	| LET UIDENT param_list '=' expression EOL
		{
			$$ = new MatchNode($2, $3, $5);
		}
	| DATA UIDENT '=' constructor_spec EOL
		{
			$$ = new DataDeclaration($2, $4);
		}
	| assignable PLUS_EQUAL expression EOL
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new AssignNode($1->clone(), new FunctionCallNode("+", argList));
		}
	| assignable MINUS_EQUAL expression EOL
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new AssignNode($1->clone(), new FunctionCallNode("-", argList));
		}
	| assignable TIMES_EQUAL expression EOL
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new AssignNode($1->clone(), new FunctionCallNode("*", argList));
		}
	| assignable DIV_EQUAL expression EOL
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new AssignNode($1->clone(), new FunctionCallNode("/", argList));
		}
	| DEF ident parameters optionaltype '=' suite
		{
			$$ = new FunctionDefNode($2, $6, $3, $4);
		}
	| STRUCT UIDENT '=' struct_def
		{
			$$ = new StructDefNode($2, $4);
		}
	| FOREIGN ident parameters DCOLON typedecl EOL
		{
			$$ = new ForeignDeclNode($2, $3, $5);
		}
	| RETURN expression EOL
		{
			$$ = new ReturnNode($2);
		}
	| expression EOL
		{
			$$ = $1;
		}

constructor_spec: UIDENT
		{
			$$ = new ConstructorSpec($1);
		}
	| constructor_spec type
		{
			$1->append($2);
			$$ = $1;
		}

optionaltype: /* empty */
		{
			$$ = nullptr;
		}
	| DCOLON typedecl
		{
			$$ = $2;
		}

typedecl: type
		{
			$$ = new TypeDecl();
			$$->emplace_back($1);
		}
	| typedecl RARROW type
		{
			$$ = $1;
			$$->emplace_back($3);
		}

parameters: /* empty */
		{
			$$ = new ParamListNode();
		}
	| param_list
		{
			$$ = $1;
		}

param_list: LIDENT
		{
			$$ = new ParamListNode();
			$$->append($1);
		}
	| param_list LIDENT
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

 //// Structures ///////////////////////////////////////////////////////////////

struct_def: member_def
		{
			$$ = new MemberList();
			$$->emplace_back($1);
		}
	| EOL INDENT member_list DEDENT
		{
			$$ = $3;
		}

member_def: LIDENT DCOLON type EOL
		{
			$$ = new MemberDefNode($1, $3);
		}

member_list: member_def
		{
			$$ = new MemberList();
			$$->emplace_back($1);
		}
	| member_list member_def
		{
			$1->emplace_back($2);
			$$ = $1;
		}

 //// Expressions //////////////////////////////////////////////////////////////

expression: expression AND expression
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
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new FunctionCallNode("+", argList);
		}
	| expression '-' expression
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new FunctionCallNode("-", argList);
		}
	| expression '*' expression
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new FunctionCallNode("*", argList);
		}
	| expression '/' expression
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new FunctionCallNode("/", argList);
		}
	| expression MOD expression
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new FunctionCallNode("%", argList);
		}
	| expression ':' expression
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new FunctionCallNode("Cons", argList);
		}
	| expression CONCAT expression
		{
			ArgList* argList = new ArgList;
			argList->emplace_back($1);
			argList->emplace_back($3);

			$$ = new FunctionCallNode("concat", argList);
		}
	| simple_expression
		{
			$$ = $1;
		}
	| ident arg_list arg_list_tail
		{
			ArgList* argList = $2;
			argList->emplace_back($3);

			$$ = new FunctionCallNode($1, argList);
		}
	| UIDENT '{' '}'
		{
			$$ = new StructInitNode($1);
		}

arg_list_tail: '$' expression
		{
			$$ = $2;
		}
	| simple_expression
		{
			$$ = $1;
		}

arg_list: /* empty */
    	{
    		$$ = new ArgList();
    	}
	| arg_list simple_expression
        {
            $1->emplace_back($2);
            $$ = $1;
        }

simple_expression: '(' expression ')'
		{
			$$ = $2;
		}
	| ident
		{
			$$ = new NullaryNode($1);
		}
	| LIDENT '{' LIDENT '}'
		{
			$$ = new MemberAccessNode($1, $3);
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
	| inline_list
		{
			$$ = $1;
		}
	| STRING_LIT
		{
			$$ = new StringNode($1);
		}

assignable: LIDENT
		{
			$$ = new VariableNode($1);
		}
	| LIDENT '{' LIDENT '}'
		{
			$$ = new MemberAccessNode($1, $3);
		}

inline_list: '[' list_interior ']'
		{
			$$ = makeList($2);
		}
	| '[' ']'
		{
			$$ = new FunctionCallNode("Nil", new ArgList);
		}

list_interior: expression
        {
            $$ = new ArgList();
            $$->emplace_back($1);
        }
	| list_interior ',' expression
        {
            $1->emplace_back($3);
            $$ = $1;
        }

ident: LIDENT
		{
			$$ = $1;
		}
	| UIDENT
		{
			$$ = $1;
		}

type: UIDENT
		{
			$$ = new TypeName($1);
		}
	| '[' type ']'
		{
			$$ = new TypeName("List");
			$$->append($2);
		}

%%

void yyerror(const char* msg)
{
	std::cerr << "Near line " << yylloc.first_line << ", "
			  << "column " << yylloc.first_column << ": "
			  << msg << std::endl;
}
