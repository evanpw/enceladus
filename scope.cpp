#include <cassert>
#include "ast.hpp"
#include "scope.hpp"

using namespace std;

Symbol* Scope::find(const std::string& name)
{
	auto i = symbols.find(name);

	if (i == symbols.end())
	{
		return nullptr;
	}
	else
	{
		return i->second.get();
	}
}

bool Scope::contains(const Symbol* symbol) const
{
	for (auto& i : symbols)
	{
		if (i.second.get() == symbol) return true;
	}

	return false;
}

void Scope::insert(Symbol* symbol)
{
	assert(symbols.find(symbol->name) == symbols.end());

	symbols[symbol->name].reset(symbol);
}

Symbol* Scope::release(const std::string& name)
{
	Symbol* symbol = symbols[name].release();
	symbols.erase(name);

	return symbol;
}
