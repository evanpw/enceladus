#include <iostream>
#include <deque>
#include <stack>
#include "ast.hpp"
#include "tokens.hpp"
#include "parser.hpp"

// The scanner function produced by flex
extern int yylex_raw();

std::stack<int> indentation;
std::deque<int> token_queue;

// The scanner function seen by the parser. Handles initial whitespace
// and python-style indentation
int yylex()
{
    static int last_token = tEOL, last_returned_token = tEOL;

    // TODO: Move this initialization elsewhere, and don't do the check repeatedly.
    if (indentation.empty()) indentation.push(0);

    while (true)
    {
        // Always consume the tokens on the queue before reading new ones
        if (!token_queue.empty())
        {
            int token = token_queue.front();
            token_queue.pop_front();

            last_returned_token = last_token = token;
            return token;
        }

        int token = yylex_raw();

        // Handle unfinished indentation blocks when EOF is reached
        if (token == 0)
        {
            if (last_returned_token != tEOL) token_queue.push_back(tEOL);

            while (indentation.top() > 0)
            {
                indentation.pop();
                token_queue.push_back(tDEDENT);
            }

            token_queue.push_back(0);
            continue;
        }

        if (last_token == tEOL)
        {
            // Non-whitespace following an tEOL means that the indentation level is 0
            int new_level = 0;
            if (token == tWHITESPACE)
            {
                new_level = yylval.number;
            }

            if (new_level > indentation.top())
            {
                // Increase of indentation -> tINDENT
                indentation.push(new_level);
                token_queue.push_back(tINDENT);
                continue;
            }
            else if (new_level < indentation.top())
            {
                // Decrease of indentation -> tDEDENT(s)
                while (new_level < indentation.top())
                {
                    indentation.pop();
                    token_queue.push_back(tDEDENT);
                }

                // Dedenting to a level we have seen before is an error
                if (new_level != indentation.top())
                {
                    //TODO: throw SyntaxError("Unexpected indentation level on line X");
                    token_queue.push_back(0);
                    continue;
                }
            }

            if (token != tWHITESPACE)
            {
                token_queue.push_back(token);
            }
            else
            {
                last_token = tWHITESPACE;
            }
        }
        else
        {
            if (token != tWHITESPACE)
            {
                token_queue.push_back(token);
            }
        }
    }
}
