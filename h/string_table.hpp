#ifndef STRING_TABLE_H
#define STRING_TABLE_H

#include <iostream>
#include <string>
#include <vector>

/*
 * Stores all of the strings encountered during lexical analysis, so that the scanner and parser
 * don't have to worry about memory management.
 */
class StringTable
{
public:
	static const char* add(const std::string& str);

private:
	StringTable();
	static std::vector<std::string> strings_;
};

#endif
