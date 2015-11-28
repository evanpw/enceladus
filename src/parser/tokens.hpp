#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <string>

enum TokenType : int
{
    tNONE = -1,
    tEND = 0,

    tAND = 256,
    tAS,
    tASSERT,
    tBREAK,
    tCHAR_LIT,
    tCOLON_EQUAL,
    tCONTINUE,
    tDARROW,
    tDATA,
    tDCOLON,
    tDEDENT,
    tDEF,
    tDIV_EQUAL,
    tELIF,
    tELSE,
    tEOF,
    tEOL,
    tEQUALS,
    tFALSE,
    tFOR,
    tFOREIGN,
    tFOREVER,
    tGE,
    tIF,
    tIMPL,
    tIN,
    tINDENT,
    tINT_LIT,
    tLE,
    tLET,
    tLIDENT,
    tMATCH,
    tMINUS_EQUAL,
    tNE,
    tOR,
    tPASS,
    tPLUS_EQUAL,
    tRARROW,
    tREM_EQUAL,
    tRETURN,
    tSTRING_LIT,
    tSTRUCT,
    tTIL,
    tTIMES_EQUAL,
    tTO,
    tTRAIT,
    tTRUE,
    tTYPE,
    tUIDENT,
    tWHERE,
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
    int64_t signedInt;
    uint64_t unsignedInt;
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
