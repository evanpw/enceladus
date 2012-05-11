%{
#include "ast.hpp"
#include "string_table.hpp"
#include "simple.tab.h"

int line_number = 1;
extern YYSTYPE yylval;
%}

/* Only scan one file per execution */
%option noyywrap

%%

[1-9][0-9]*	{ yylval.symbol = StringTable::add(yytext); return INT_LIT; }
"#".*		/* Ignore comments */

 /* Operators */
"+"		{ return '+'; }
"-"		{ return '-'; }
"*"		{ return '*'; }
"/"		{ return '/'; }
">"		{ return '>'; }
"="		{ return '='; }
":"		{ return ':'; }
"("		{ return '('; }
")"		{ return ')'; }
"<-"		{ return ASSIGN; }
"not"		{ return NOT; }

 /* Keywords */
"if"		{ return IF; }
"then"		{ return THEN; }
"goto"		{ return GOTO; }
"print"		{ return PRINT; }
"read"		{ return READ; }

[a-zA-Z][a-zA-Z0-9_]*	 { yylval.symbol = StringTable::add(yytext); return IDENT; }
\n		{ ++line_number; return EOL; }
[ \t\r]		/* Ignore whitespace */
.		{
			char msg[32];
			sprintf(msg, "Unknown character: %c", yytext[0]);
			yylval.symbol = StringTable::add(msg);
			return ERROR;
		}

%%