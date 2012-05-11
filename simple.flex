%{
#include "string_table.hpp"
#define YYSTYPE const char*
#include "simple.tab.h"

int line_number = 1;
extern YYSTYPE yylval;
%}

/* Only scan one file per execution */
%option noyywrap

%%

[1-9][0-9]*	{ yylval = StringTable::add(yytext); return INT_LIT; }
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

[a-zA-Z][a-zA-Z0-9_]*	 { yylval = StringTable::add(yytext); return IDENT; }
\n		{ ++line_number; return EOL; }
[ \t\r]		/* Ignore whitespace */
.		{
			char msg[32];
			sprintf(msg, "Unknown character: %c", yytext[0]);
			yylval = StringTable::add(msg);
			return ERROR;
		}

%%