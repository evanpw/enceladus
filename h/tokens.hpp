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
    tDO,
    tELSE,
    tEOL,
    tEQUALS,
    tFALSE,
    tFOR,
    tFOREIGN,
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
    tTHEN,
    tTIMES_EQUAL,
    tTRUE,
    tTYPE,
    tUIDENT,
    tVAR,
    tWHILE,
    tWHITESPACE,
};

std::string tokenToString(TokenType t);

#endif
