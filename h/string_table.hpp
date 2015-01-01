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
    ~StringTable();

	static const char* add(const std::string& str);

private:
    static StringTable& getInstance();

	std::vector<const char*> _strings;
};

#endif
