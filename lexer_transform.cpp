#include <iostream>
#include <deque>
#include <stack>
#include "ast.hpp"
#include "simple.tab.h"

// The scanner function produced by flex
extern int yylex_raw();

std::stack<int> indentation;
std::deque<int> token_queue;

const char* token_to_string(int token)
{
    static char short_one[2] = {0, 0};

    switch (token)
    {
        case ERROR: return "ERROR";
        case IF: return "IF";
        case THEN: return "THEN";
        case ELSE: return "ELSE";
        case GOTO: return "GOTO";
        case PRINT: return "PRINT";
        case READ: return "READ";
        case ASSIGN: return "ASSIGN";
        case NOT: return "NOT";
        case AND: return "AND";
        case OR: return "OR";
        case EOL: return "EOL";
        case MOD: return "MOD";
        case WHILE: return "WHILE";
        case DO: return "DO";
        case INDENT: return "INDENT";
        case DEDENT: return "DEDENT";
        case IDENT: return "IDENT";
        case INT_LIT: return "INT_LIT";
        case WHITESPACE: return "WHITESPACE";
        case GE: return "GE";
        case LE: return "LE";
        default: short_one[0] = token; return short_one;
    }
}

const bool DEBUG_LEXER = false;

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

            if (DEBUG_LEXER) std::cerr << token_to_string(token) << std::endl;
            last_returned_token = last_token = token;
            return token;
        }

        int token = yylex_raw();

        if (DEBUG_LEXER) std::cerr << "Reading token " << token_to_string(token) << ", last = " << token_to_string(last_token) << std::endl;

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
                if (DEBUG_LEXER) std::cerr << "INDENT" << std::endl;
                token_queue.push_back(INDENT);
                continue;
            }
            else if (new_level < indentation.top())
            {
                if (DEBUG_LEXER) std::cerr << "Dedenting from " << indentation.top() << " to " << new_level << std::endl;

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
                    if (DEBUG_LEXER) std::cerr << "Unexpected indentation" << std::endl;
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
                if (DEBUG_LEXER) std::cerr << token_to_string(token) << std::endl;
                token_queue.push_back(token);
            }
        }
    }
}