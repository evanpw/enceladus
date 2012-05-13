#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <iostream>

inline void indent(std::ostream& out, int level)
{
	for (int k = 0; k < level; ++k) out << "\t";
}

#endif
