#include <iostream>
#include "ast.hpp"
#include "string_table.hpp"
#include "simple.tab.h"

extern int line_number;
YYSTYPE yylval;

const char* token_to_string(int token)
{
	switch (token)
	{
	case 0: return "EOF"; break;
	case IF: return "IF"; break;
	case THEN: return "THEN"; break;
	case GOTO: return "GOTO"; break;
	case PRINT: return "PRINT"; break;
	case READ: return "READ"; break;
	case ASSIGN: return "ASSIGN"; break;
	case NOT: return "NOT"; break;
	case INT_LIT: return "INT_LIT"; break;
	case IDENT: return "IDENT"; break;
	case ERROR: return "ERROR"; break;
	case EOL: return "EOL"; break;
	case '+': return "'+'"; break;
	case '-': return "'-'"; break;
	case '*': return "'*'"; break;
	case '/': return "'/'"; break;
	case '>': return "'>'"; break;
	case '=': return "'='"; break;
	case ':': return "':'"; break;
	case '(': return "'('"; break;
	case ')': return "')'"; break;
	default: return "UNKNOWN"; break;
	}
}

int yylex();

using namespace std;

int main()
{
	int token;
	while ((token = yylex()) != 0)
	{
		if (token == EOL)
		{
			cout << "#" << (line_number - 1) << " " << token_to_string(token) << endl;
		}
		else
		{
			cout << "#" << line_number << " ";
			switch (token)
			{
			case ERROR:
			case IDENT:
			case INT_LIT:
				cout << token_to_string(token) << ": " << yylval.symbol << endl;
				break;
			default:
				cout << token_to_string(token) << endl;
				break;
			}
		}
	}

	return 0;
}
