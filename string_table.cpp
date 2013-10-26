#include <iostream>
#include <utility>
#include "string_table.hpp"

std::vector<std::string> StringTable::strings_;

const char* StringTable::add(const char* str)
{
	// Horribly inefficient
	std::string new_string = str;
	for (auto& i : strings_)
	{
		if (new_string == i)
		{
			return i.c_str();
		}
	}


	strings_.push_back(std::move(new_string));
	return strings_.back().c_str();
}
