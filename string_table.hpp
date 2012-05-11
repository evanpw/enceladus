#ifndef STRING_TABLE_H
#define STRING_TABLE_H

#include <string>
#include <vector>

/*
 * Singleton class that stores all of the strings encountered during
 * lexical analysis, so that the scanner and parser don't have to
 * worry about memory management.
 */
class StringTable
{
public:
	static const char* add(const char* str);

private:
	// Singleton interface
	StringTable() {}
	static StringTable* instance();
	
	std::vector<std::string> strings_;
};

#endif
