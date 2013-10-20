%{
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "ast.hpp"
#include "string_table.hpp"
#include "simple.tab.h"

extern YYSTYPE yylval;

#define YY_DECL int yylex_raw()

int count_whitespace(const char* s, int length);

int yycolumn = 1;
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno; \
	yylloc.first_column = yycolumn; yylloc.last_column = yycolumn + yyleng - 1; \
	yycolumn += yyleng;
%}

%option noyywrap
%option yylineno

%%

					/* It's easier to get rid of blank lines here than in the grammar. */
^[ \t]*\n         	{ yycolumn = 1; }
^[ \t]*"#".+\n 	  	{ yycolumn = 1; }

-?[0-9][0-9]*	{
					try
					{
						yylval.number = boost::lexical_cast<long>(yytext);
						return INT_LIT;
					}
					catch (boost::bad_lexical_cast&)
					{
						std::cerr << "Near line " << yylloc.first_line << ", "
						 		  << "column " << yylloc.first_column << ": "
						 		  << "error: integer literal out of range: " << yytext << std::endl;

						return ERROR;
					}
				}
"#".*			/* Ignore comments */

 /* Operators */
"+"		{ return '+'; }
"-"		{ return '-'; }
"*"		{ return '*'; }
"/"		{ return '/'; }
"mod"	{ return MOD; }
">"		{ return '>'; }
"<"		{ return '<'; }
"<="	{ return LE; }
">="	{ return GE; }
"=="	{ return EQUALS; }
"!="	{ return NE; }
":"		{ return ':'; }
"("		{ return '('; }
")"		{ return ')'; }
","		{ return ','; }
"="		{ return '='; }
"::"	{ return DCOLON; }
"not"	{ return NOT; }
"and"	{ return AND; }
"or"	{ return OR; }

 /* Keywords */
"if"		{ return IF; }
"then"		{ return THEN; }
"else"		{ return ELSE; }
"print"		{ return PRINT; }
"read"		{ return READ; }
"while"		{ return WHILE; }
"do"		{ return DO; }
"def"		{ return DEF; }
"return"	{ return RETURN; }
"True"		{ return TRUE; }
"False"		{ return FALSE; }

[a-zA-Z][a-zA-Z0-9_]*	 { yylval.str = StringTable::add(yytext); return IDENT; }
\n						 { yycolumn = 1; return EOL; }
[ \t]+				     { yylval.number = count_whitespace(yytext, yyleng); return WHITESPACE; }
.						 {
						 	std::cerr << "Near line " << yylloc.first_line << ", "
						 			  << "column " << yylloc.first_column << ": "
						 			  << "error: stray '" << yytext[0] << "'" << std::endl;

						 	return ERROR;
						 }

%%

int count_whitespace(const char* s, int length)
{
	int count = 0;
	for (int i = 0; i < length; ++i)
	{
		if (s[i] == ' ') count += 1;
		if (s[i] == '\t') count += 4;
	}

	return count;
}
