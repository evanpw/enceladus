%{
#include "ast/ast.hpp"
#include "exceptions.hpp"
#include "lexer/string_table.hpp"
#include "parser/tokens.hpp"

#include <iostream>
#include <sstream>
#include <stack>
#include <boost/lexical_cast.hpp>

#define YY_DECL int yylex_raw()

std::string trim_quotes(const std::string& str)
{
    return str.substr(1, str.length() - 2);
}

std::string trim_right(const std::string str)
{
    return str.substr(0, str.length() - 1);
}

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

char char_literal(const char* s)
{
    if (s[1] != '\\')
    {
        return s[1];
    }
    else if (s[2] == 'n')
    {
        return '\n';
    }
    else if (s[2] == 'r')
    {
        return '\r';
    }
    else if (s[2] == 't')
    {
        return '\t';
    }
    else if (s[2] == '\\')
    {
        return '\\';
    }
    else
    {
        return 0;
    }
}

int unclosedBrackets = 0;

int yycolumn = 1;
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; yylloc.last_column = yycolumn + yyleng - 1; \
    yylloc.filename = currentFilename; yycolumn += yyleng;


std::stack<std::pair<int, int>> locationStack;
std::stack<const char*> filenameStack;
const char* currentFilename;

void initializeLexer(const std::string& fileName)
{
    yyin = fopen(fileName.c_str(), "r");
    assert(yyin);

    filenameStack.push(NULL);
    filenameStack.push(StringTable::add(fileName.c_str()));

    yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));

    yylineno = 1;
    yycolumn = 1;
    currentFilename = filenameStack.top();

    BEGIN(0);
}

bool importFile(const std::string& fileName)
{
    yyin = fopen(fileName.c_str(), "r");
    if (!yyin) return false;

    filenameStack.push(StringTable::add(fileName.c_str()));

    yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));
    locationStack.push({yylineno, yycolumn});

    yylineno = 1;
    yycolumn = 1;
    currentFilename = filenameStack.top();

    BEGIN(0);
    return true;
}

extern int yylex_destroy();

void shutdownLexer()
{
    while (1)
    {
        if (yyin)
        {
            fclose(yyin);
        }

        yypop_buffer_state();

        if (!YY_CURRENT_BUFFER)
        {
            yylex_destroy();
            return;
        }
    }
}

extern "C" int yywrap()
{
    return 1;
}

%}

%option yylineno

%x import

%%

 /* It's easier to get rid of blank lines here than in the grammar. */
^[ \t]*("#".*)?\n  { yycolumn = 1; }

-?[0-9][0-9]*(u|i|u8)? { yylval.str = StringTable::add(yytext); return tINT_LIT; }

 /* Python-style comments */
"#".*

 /* Operators and punctuation */
"+"     { return '+'; }
"-"     { return '-'; }
"*"     { return '*'; }
"/"     { return '/'; }
"%"     { return '%'; }
">"     { return '>'; }
"<"     { return '<'; }
":"     { return ':'; }
"("     { return '('; }
")"     { return ')'; }
","     { return ','; }
"="     { return '='; }
"."     { return '.'; }
":="    { return tCOLON_EQUAL; }
"$"     { return '$'; }
"["     { ++unclosedBrackets; return '['; }
"]"     { unclosedBrackets = std::max(0, unclosedBrackets - 1); return ']'; }
"{"     { return '{'; }
"}"     { return '}'; }
"|"     { return '|'; }
"->"    { return tRARROW; }
"=>"    { return tDARROW; }
"<="    { return tLE; }
">="    { return tGE; }
"=="    { return tEQUALS; }
"!="    { return tNE; }
"::"    { return tDCOLON; }
"and"   { return tAND; }
"or"    { return tOR; }

 /* Syntactic sugar */
"+="    { return tPLUS_EQUAL; }
"-="    { return tMINUS_EQUAL; }
"*="    { return tTIMES_EQUAL; }
"/="    { return tDIV_EQUAL; }
"%="    { return tREM_EQUAL; }

 /* Keywords */
"assert"    { return tASSERT; }
"as"        { return tAS; }
"break"     { return tBREAK; }
"continue"  { return tCONTINUE; }
"enum"      { return tENUM; }
"def"       { return tDEF; }
"import"    { BEGIN(import); }
"elif"      { return tELIF; }
"else"      { return tELSE; }
"False"     { return tFALSE; }
"for"       { return tFOR; }
"foreign"   { return tFOREIGN; }
"forever"   { return tFOREVER; }
"if"        { return tIF; }
"impl"      { return tIMPL; }
"in"        { return tIN; }
"let"       { return tLET; }
"match"     { return tMATCH; }
"pass"      { return tPASS; }
"return"    { return tRETURN; }
"struct"    { return tSTRUCT; }
"til"       { return tTIL; }
"to"        { return tTO; }
"True"      { return tTRUE; }
"trait"     { return tTRAIT; }
"type"      { return tTYPE; }
"where"     { return tWHERE; }
"while"     { return tWHILE; }

 /* Eat whitespace */
<import>[ \t]*

 /* Import file name */
<import>[^ \t\n]+\n {

    bool result = importFile(std::string("lib/") + trim_right(yytext) + ".enc");

    if (!result)
    {
        std::stringstream ss;
        ss << yylloc.filename << ":" << yylloc.first_line << ":" << yylloc.first_column
           << ": can't import `" << trim_right(yytext) << "`: file not found";

        throw LexerError(ss.str());
    }
}

<<EOF>> {
    if (yyin)
    {
        fclose(yyin);
        yyin = NULL;
    }

    yypop_buffer_state();

    if (!locationStack.empty())
    {
        std::pair<int, int> prevLocation = locationStack.top();
        locationStack.pop();
        yylineno = prevLocation.first;
        yycolumn = prevLocation.second;
    }

    filenameStack.pop();
    currentFilename = filenameStack.top();

    if (!YY_CURRENT_BUFFER)
    {
        yyterminate();
    }

    return tEOF;
}

 /* String and char literals */
\"[^"]*\"                { yylval.str = StringTable::add(trim_quotes(yytext)); return tSTRING_LIT; }

\'([^']|\\.)\'       {
    char c = char_literal(yytext);

    if (c)
    {
        yylval.unsignedInt = c;
        return tCHAR_LIT;
    }
    else
    {
        std::stringstream ss;
        ss << yylloc.filename << ":" << yylloc.first_line << ":" << yylloc.first_column
           << ": bad character literal: " << yytext;

        throw LexerError(ss.str());
    }
}

[a-z][a-zA-Z0-9']*       { yylval.str = StringTable::add(yytext); return tLIDENT; }
"_"                      { yylval.str = StringTable::add(yytext); return tLIDENT; }
[A-Z][a-zA-Z0-9']*       { yylval.str = StringTable::add(yytext); return tUIDENT; }
\\\n                     { yycolumn = 1; }
\n                       { yycolumn = 1; if (unclosedBrackets == 0) return tEOL; }
[ \t]+                   { yylval.unsignedInt = count_whitespace(yytext, yyleng); return tWHITESPACE; }

.                        {
                            std::stringstream ss;
                            ss << yylloc.filename << ":" << yylloc.first_line << ":" << yylloc.first_column
                               << ": stray '" << yytext[0] << "'";

                            throw LexerError(ss.str());
                         }

%%

