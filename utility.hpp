#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <algorithm>
#include <set>

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

#endif
