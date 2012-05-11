#include <iostream>
#include <string>
#include "string_table.hpp"

using namespace std;

const char* StringTable::add(const char* str)
{
	string copied(str);
	instance()->strings_.push_back(copied);
	
	return copied.c_str();
}

StringTable* StringTable::instance()
{
	static StringTable string_table;
	return &string_table;
}
