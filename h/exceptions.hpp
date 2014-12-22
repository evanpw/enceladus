#ifndef EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

struct LexerError : std::runtime_error
{
    explicit LexerError(const std::string& msg)
    : std::runtime_error(msg)
    {}
};

#endif