#ifndef STRING_TABLE_H
#define STRING_TABLE_H

#include <iostream>
#include <string>
#include <vector>

class Symbol
{
public:
	Symbol(const std::string& value);
	void show(std::ostream& out, int indent = 0) const;
	
private:
	std::string value_;
};

/*
 * Singleton class that stores all of the strings encountered during
 * lexical analysis, so that the scanner and parser don't have to
 * worry about memory management.
 */
class StringTable
{
public:
	static Symbol* add(const char* str);
	static void freeStrings();

private:
	StringTable();
	static std::vector<Symbol*> symbols_;
};

#endif
