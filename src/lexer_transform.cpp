#include <iostream>
#include <deque>
#include <stack>
#include "ast.hpp"
#include "tokens.hpp"
#include "parser.hpp"

// The scanner function produced by flex
extern int yylex_raw();

// Set by the flex lexer
YYLTYPE yylloc;
YYSTYPE yylval;

std::stack<int> indentation({0});
std::deque<Token> token_queue;

// The scanner function seen by the parser. Handles initial whitespace
// and python-style indentation
Token yylex()
{
    static Token last_token(tEOL);
    static Token last_returned_token(tEOL);

    while (true)
    {
        // Always consume the tokens on the queue before reading new ones
        if (!token_queue.empty())
        {
            Token token = token_queue.front();
            token_queue.pop_front();

            last_returned_token = last_token = token;
            return token;
        }

        Token token;
        token.type = (TokenType)yylex_raw();
        token.value = yylval;
        token.location = yylloc;

        // Handle unfinished indentation blocks when EOF is reached
        if (token.type == 0)
        {
            if (last_returned_token.type != tEOL)
                token_queue.emplace_back(tEOL, token.location);

            while (indentation.top() > 0)
            {
                indentation.pop();
                token_queue.emplace_back(tDEDENT, token.location);
            }

            token_queue.emplace_back(tEOF, token.location);
            continue;
        }

        if (last_token.type == tEOL)
        {
            // Non-whitespace following an tEOL means that the indentation level is 0
            int new_level = 0;
            if (token.type == tWHITESPACE)
            {
                new_level = token.value.number;
            }

            if (new_level > indentation.top())
            {
                // Increase of indentation -> tINDENT
                indentation.push(new_level);
                token_queue.emplace_back(tINDENT, token.location);
                continue;
            }
            else if (new_level < indentation.top())
            {
                // Decrease of indentation -> tDEDENT(s)
                while (new_level < indentation.top())
                {
                    indentation.pop();
                    token_queue.emplace_back(tDEDENT, token.location);
                }

                // Dedenting to a level we have seen before is an error
                if (new_level != indentation.top())
                {
                    //TODO: throw SyntaxError("Unexpected indentation level on line X");
                    token_queue.emplace_back(tEOF, token.location);
                    continue;
                }
            }

            if (token.type != tWHITESPACE)
            {
                token_queue.push_back(token);
            }
            else
            {
                last_token = Token(tWHITESPACE);
            }
        }
        else
        {
            if (token.type != tWHITESPACE)
            {
                token_queue.push_back(token);
            }
        }
    }
}
