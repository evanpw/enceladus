#include <iostream>
#include <string>
#include "string_table.hpp"

using namespace std;

Symbol::Symbol(const string& value)
: value_(value)
{
}

void Symbol::show(std::ostream& out, int indent) const
{
	for (int k = 0; k < indent; ++k) out << "\t";
	out << "Symbol: " << value_ << endl;
}

std::vector<Symbol*> StringTable::symbols_;

Symbol* StringTable::add(const char* str)
{
	Symbol* symbol = new Symbol(str);
	symbols_.push_back(symbol);
	
	return symbol;	
}

void StringTable::freeStrings()
{
	for (vector<Symbol*>::iterator i = symbols_.begin(); i != symbols_.end(); ++i)
	{
		delete *i;
	}
	
	symbols_.clear();
}
