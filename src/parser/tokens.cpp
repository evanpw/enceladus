#include "parser/tokens.hpp"

std::string tokenToString(TokenType t)
{
    switch(t)
    {
    case tNONE:
        return "`None`";
    case tAND:
        return "`and`";
    case tAS:
        return "`and`";
    case tASSERT:
        return "`assert`";
    case tBREAK:
        return "`break`";
    case tCHAR_LIT:
        return "character literal";
    case tCOLON_EQUAL:
        return "`:=`";
    case tCONTINUE:
        return "`continue`";
    case tDARROW:
        return "`=>`";
    case tDATA:
        return "`data`";
    case tDCOLON:
        return "`::`";
    case tDEDENT:
        return "dedentation";
    case tDEF:
        return "`def`";
    case tDIV_EQUAL:
        return "`/=`";
    case tELIF:
        return "`elif`";
    case tELSE:
        return "`else`";
    case tEND:
        return "end-of-input";
    case tEOF:
        return "end-of-file";
    case tEOL:
        return "end-of-line";
    case tEQUALS:
        return "`==`";
    case tFALSE:
        return "`False`";
    case tFOR:
        return "`for`";
    case tFOREIGN:
        return "`foreign`";
    case tFOREVER:
        return "`forever`";
    case tGE:
        return "`>=`";
    case tIF:
        return "`if`";
    case tIMPL:
        return "`impl`";
    case tIN:
        return "in";
    case tINDENT:
        return "indentation";
    case tINT_LIT:
        return "integer literal";
    case tLE:
        return "`<=`";
    case tLET:
        return "`let`";
    case tLIDENT:
        return "identifier";
    case tMATCH:
        return "`match`";
    case tMINUS_EQUAL:
        return "`*=`";
    case tNE:
        return "`!=`";
    case tOR:
        return "`or`";
    case tPLUS_EQUAL:
        return "`+=`";
    case tRARROW:
        return "`->`";
    case tREM_EQUAL:
        return "`%=`";
    case tRETURN:
        return "`return`";
    case tSTRING_LIT:
        return "string literal";
    case tSTRUCT:
        return "`struct`";
    case tTIMES_EQUAL:
        return "`*=`";
    case tTO:
        return "`to`";
    case tTRAIT:
        return "`trait`";
    case tTRUE:
        return "`True`";
    case tTYPE:
        return "`type`";
    case tUIDENT:
        return "type identifier";
    case tWHERE:
        return "`where`";
    case tWHILE:
        return "`while`";
    case tWHITESPACE:
        return "whitespace";
    default:
        char c = (char)t;
        return std::string(1, c);
    }
}
