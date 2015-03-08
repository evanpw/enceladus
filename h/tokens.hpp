#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <string>

enum TokenType : int
{
    tNONE = -1,
    tEOF = 0,

    tAND = 256,
    tBREAK,
    tCOLON_EQUAL,
    tCONCAT,
    tDATA,
    tDCOLON,
    tDEDENT,
    tDEF,
    tDIV_EQUAL,
    tELIF,
    tELSE,
    tEOL,
    tEQUALS,
    tFALSE,
    tFOR,
    tFOREIGN,
    tFOREVER,
    tGE,
    tIF,
    tIN,
    tINDENT,
    tINT_LIT,
    tLE,
    tLET,
    tLIDENT,
    tMINUS_EQUAL,
    tMOD,
    tNE,
    tOR,
    tPLUS_EQUAL,
    tRARROW,
    tRETURN,
    tSTRING_LIT,
    tSTRUCT,
    tTIMES_EQUAL,
    tTRUE,
    tTYPE,
    tUIDENT,
    tVAR,
    tWHILE,
    tWHITESPACE,
};

std::string tokenToString(TokenType t);

struct YYLTYPE
{
    int first_line;
    int first_column;
    int last_line;
    int last_column;
};

extern YYLTYPE yylloc;

union YYSTYPE
{
    const char* str;
    long number;
};

extern YYSTYPE yylval;

struct Token
{
    Token(TokenType type = tNONE) : type(type) {}

    TokenType type;
    YYSTYPE value;
    YYLTYPE location;
};

#endif
