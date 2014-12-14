#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <algorithm>
#include <cassert>
#include <memory>
#include <set>
#include <sstream>

template <class T>
std::set<T>& operator+=(std::set<T>& lhs, const std::set<T>& rhs)
{
    std::set_union(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::inserter(lhs, lhs.end()));
    return lhs;
}

template <class T>
std::set<T> operator-(const std::set<T>& lhs, const std::set<T>& rhs)
{
    std::set<T> result;
    std::set_difference(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::inserter(result, result.end()));

    return result;
}

template<class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

std::string format(const std::string& str);

template<typename T, typename... Args>
std::string format(const std::string& str, T value, Args... args)
{
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '{' && i + 1 < str.length() && str[i + 1] == '}')
        {
            std::stringstream ss;
            ss << str.substr(0, i) << value << format(str.substr(i + 2), args...);
            return ss.str();
        }
    }

    assert(false);
}

#endif
