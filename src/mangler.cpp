#include "mangler.hpp"
#include <sstream>

std::string mangle(const std::string& name)
{
    std::stringstream ss;
    ss << "_Z" << name.length() << name;
    return ss.str();
}
