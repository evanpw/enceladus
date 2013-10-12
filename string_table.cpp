#include <iostream>
#include <cstring>
#include "string_table.hpp"

using namespace std;

vector<const char*> StringTable::strings_;

const char* StringTable::add(const char* str)
{
	// Horribly inefficient
	for (vector<const char*>::iterator i = strings_.begin(); i != strings_.end(); ++i)
	{
		if (strcmp(*i, str) == 0)
		{
			return *i;
		}
	}

	char* copy = new char[strlen(str) + 1];
	strcpy(copy, str);
	strings_.push_back(copy);

	return copy;
}

void StringTable::freeStrings()
{
	for (vector<const char*>::iterator i = strings_.begin(); i != strings_.end(); ++i)
	{
		delete[] *i;
	}

	strings_.clear();
}
