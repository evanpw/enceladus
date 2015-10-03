#ifndef VALUE_TYPE_HPP
#define VALUE_TYPE_HPP

#include <cassert>

enum class ValueType { Reference, I64, U64, NonHeapAddress };

static inline const char* valueTypeString(ValueType type)
{
    switch (type)
    {
        case ValueType::Reference:
            return "Reference";

        case ValueType::I64:
            return "I64";

        case ValueType::U64:
            return "U64";

        case ValueType::NonHeapAddress:
            return "NonHeapAddress";
    }

    assert(false);
}

inline bool isInteger(ValueType type)
{
    switch (type)
    {
        case ValueType::I64:
        case ValueType::U64:
            return true;

        default:
            return false;
    }
}

inline bool isSigned(ValueType type)
{
    switch (type)
    {
        case ValueType::I64:
            return true;

        case ValueType::U64:
            return false;

        default:
            assert(false);
    }
}

#endif
