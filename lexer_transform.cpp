#include <iostream>
#include <deque>
#include <stack>
#include "ast.hpp"
#include "simple.tab.h"

// The scanner function produced by flex
extern int yylex_raw();

std::stack<int> indentation;
std::deque<int> token_queue;

// The scanner function seen by the parser. Handles initial whitespace
// and python-style indentation
int yylex()
{
    static int last_token = EOL, last_returned_token = EOL;

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
        //std::cerr << token << " " << yylloc.first_line << " " << yylloc.first_column << std::endl;

        // Handle unfinished indentation blocks when EOF is reached
        if (token == 0)
        {
            if (last_returned_token != EOL) token_queue.push_back(EOL);

            while (indentation.top() > 0)
            {
                indentation.pop();
                token_queue.push_back(DEDENT);
            }

            token_queue.push_back(0);
            continue;
        }

        if (last_token == EOL)
        {
            // Non-whitespace following an EOL means that the indentation level is 0
            int new_level = 0;
            if (token == WHITESPACE)
            {
                new_level = yylval.number;
            }

            if (new_level > indentation.top())
            {
                // Increase of indentation -> INDENT
                indentation.push(new_level);
                token_queue.push_back(INDENT);
                continue;
            }
            else if (new_level < indentation.top())
            {
                // Decrease of indentation -> DEDENT(s)
                while (new_level < indentation.top())
                {
                    indentation.pop();
                    token_queue.push_back(DEDENT);
                }

                // Dedenting to a level we have seen before is an error
                if (new_level != indentation.top())
                {
                    //TODO: throw SyntaxError("Unexpected indentation level on line X");
                    token_queue.push_back(0);
                    continue;
                }
            }

            if (token != WHITESPACE)
            {
                token_queue.push_back(token);
            }
            else
            {
                last_token = WHITESPACE;
            }
        }
        else
        {
            if (token != WHITESPACE)
            {
                token_queue.push_back(token);
            }
        }
    }
}
