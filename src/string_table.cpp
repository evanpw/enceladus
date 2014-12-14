#include <iostream>
#include <utility>
#include "string_table.hpp"

std::vector<std::string> StringTable::strings_;

const char* StringTable::add(const std::string& str)
{
	// Horribly inefficient
	for (auto& i : strings_)
	{
		if (i == str)
		{
			return i.c_str();
		}
	}


	strings_.push_back(str);
	return strings_.back().c_str();
}
