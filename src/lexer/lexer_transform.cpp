#include "ast/ast.hpp"
#include "parser/parser.hpp"
#include "parser/tokens.hpp"

#include <iostream>
#include <deque>
#include <stack>

// The scanner function produced by flex
extern int yylex_raw();

// Set by the flex lexer
YYLTYPE yylloc;
YYSTYPE yylval;

std::stack<int> indentation({0});
std::deque<Token> tokenQueue;

// The scanner function seen by the parser. Handles initial whitespace
// and python-style indentation
Token yylex()
{
    static Token lastToken(tEOL);
    static Token lastReturnedToken(tEOL);
    static bool finished = false;

    if (finished)
    {
        return lastReturnedToken;
    }

    while (true)
    {
        // Always consume the tokens on the queue before reading new ones
        if (!tokenQueue.empty())
        {
            Token token = tokenQueue.front();
            tokenQueue.pop_front();

            lastReturnedToken = lastToken = token;

            if (token.type == tEND)
                finished = true;

            return token;
        }

        Token token;
        token.type = (TokenType)yylex_raw();
        token.value = yylval;
        token.location = yylloc;

        // Handle unfinished indentation blocks when EOF is reached
        if (token.type == tEOF || token.type == tEND)
        {
            if (lastReturnedToken.type != tEOL)
                tokenQueue.emplace_back(tEOL, token.location);

            while (indentation.top() > 0)
            {
                indentation.pop();
                tokenQueue.emplace_back(tDEDENT, token.location);
            }

            if (token.type == tEND)
                tokenQueue.push_back(token);

            continue;
        }

        if (lastToken.type == tEOL)
        {
            // Non-whitespace following an tEOL means that the indentation level is 0
            int newLevel = 0;
            if (token.type == tWHITESPACE)
            {
                newLevel = token.value.unsignedInt;
            }

            if (newLevel > indentation.top())
            {
                // Increase of indentation -> tINDENT
                indentation.push(newLevel);
                tokenQueue.emplace_back(tINDENT, token.location);
                continue;
            }
            else if (newLevel < indentation.top())
            {
                // Decrease of indentation -> tDEDENT(s)
                while (newLevel < indentation.top())
                {
                    indentation.pop();
                    tokenQueue.emplace_back(tDEDENT, token.location);
                }

                // Dedenting to a level we have seen before is an error
                if (newLevel != indentation.top())
                {
                    //TODO: throw SyntaxError("Unexpected indentation level on line X");
                    tokenQueue.emplace_back(tEOF, token.location);
                    continue;
                }
            }

            if (token.type != tWHITESPACE)
            {
                tokenQueue.push_back(token);
            }
            else
            {
                lastToken = Token(tWHITESPACE);
            }
        }
        else
        {
            if (token.type != tWHITESPACE)
            {
                tokenQueue.push_back(token);
            }
        }
    }
}
