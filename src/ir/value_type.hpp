#ifndef VALUE_TYPE_HPP
#define VALUE_TYPE_HPP

#include <cassert>
#include <cstring>

enum class ValueType { Reference, I64, U64, U32, U16, U8, NonHeapAddress };

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

        case ValueType::U32:
            return "U32";

        case ValueType::U16:
            return "U16";

        case ValueType::U8:
            return "U8";

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
        case ValueType::U32:
        case ValueType::U16:
        case ValueType::U8:
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
        case ValueType::U32:
        case ValueType::U16:
        case ValueType::U8:
            return false;

        default:
            assert(false);
    }
}

inline size_t getSize(ValueType type)
{
    switch (type)
    {
        case ValueType::I64:
        case ValueType::U64:
        case ValueType::Reference:
        case ValueType::NonHeapAddress:
            return 64;

        case ValueType::U32:
            return 32;

        case ValueType::U16:
            return 16;

        case ValueType::U8:
            return 8;

        default:
            assert(false);
    }
}

#endif
