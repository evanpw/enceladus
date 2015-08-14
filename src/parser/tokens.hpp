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
    tDARROW,
    tDATA,
    tDCOLON,
    tDEDENT,
    tDEF,
    tDIV_EQUAL,
    tDOT_BRACKET,
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
    tMATCH,
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
    tTO,
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
    const char* filename;
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
    Token(TokenType type = tNONE, YYLTYPE location = {})
    : type(type), location(location)
    {}

    TokenType type;
    YYSTYPE value;
    YYLTYPE location;
};

#endif
