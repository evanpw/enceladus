#include <cassert>
#include "ast.hpp"
#include "scope.hpp"

using namespace std;

Symbol* Scope::find(const char* name)
{
	auto i = symbols_.find(name);

	if (i == symbols_.end())
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
	for (auto& i : symbols_)
	{
		if (i.second.get() == symbol) return true;
	}

	return false;
}

void Scope::insert(Symbol* symbol)
{
	assert(symbols_.find(symbol->name) == symbols_.end());

	symbols_[symbol->name].reset(symbol);
}
