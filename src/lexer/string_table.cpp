#include "lexer/string_table.hpp"

#include <cstring>
#include <iostream>
#include <utility>

StringTable& StringTable::getInstance()
{
	static StringTable instance;
	return instance;
}

StringTable::~StringTable()
{
	for (auto& i : _strings)
	{
		delete[] i;
	}
}

const char* StringTable::add(const std::string& str)
{
	std::vector<const char*>& strings = getInstance()._strings;

	// Horribly inefficient
	for (auto& i : strings)
	{
		if (strcmp(i, str.c_str()) == 0)
		{
			return i;
		}
	}

	// Insert a copy into the table
	char* copy = new char[str.length() + 1];
	strcpy(copy, str.c_str());
	strings.push_back(copy);

	return strings.back();
}
