%{
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "ast.hpp"
#include "string_table.hpp"
#include "simple.tab.h"

extern YYSTYPE yylval;

int yycolumn = 1;
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno; \
	yylloc.first_column = yycolumn; yylloc.last_column = yycolumn + yyleng - 1; \
	yycolumn += yyleng;
%}

%option noyywrap
%option yylineno

%%

[0-9][0-9]*		{
					try
					{
						yylval.number = boost::lexical_cast<int>(yytext);
					}
					catch (boost::bad_lexical_cast&)
					{
						yylval.number = INT_MAX;
						std::cerr << "Near line " << yylloc.first_line << ", "
						 		  << "column " << yylloc.first_column << ": "
						 		  << "error: integer literal out of range: " << yytext << std::endl;
					}
					
					return INT_LIT;
				}
"#".*			/* Ignore comments */

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
"{"		{ return '{'; }
"}"		{ return '}'; }
"<-"	{ return ASSIGN; }
"not"	{ return NOT; }

 /* Keywords */
"if"		{ return IF; }
"then"		{ return THEN; }
"else"		{ return ELSE; }
"goto"		{ return GOTO; }
"print"		{ return PRINT; }
"read"		{ return READ; }

[a-zA-Z][a-zA-Z0-9_]*	 { yylval.str = StringTable::add(yytext); return IDENT; }
\n						 { yycolumn = 1; return EOL; }
[ \t\r]					 /* Ignore whitespace */
.						 { 
						 	std::cerr << "Near line " << yylloc.first_line << ", "
						 			  << "column " << yylloc.first_column << ": "
						 			  << "error: stray '" << yytext[0] << "'" << std::endl;
						 }

%%