#pragma once

#include "error.h"

#include <string>

namespace redis {

#define FOR_ALL_RESP_TYPES \
    XX(SimpleString, '+')  \
    XX(Int, ':')           \
    XX(Error, '-')         \
    XX(BulkString, '$')    \
    XX(Array, '*')         \
    XX(Empty, '\0')

enum class ERespType {
#define XX(name, c) name,
    FOR_ALL_RESP_TYPES
#undef XX
};

inline ERespType CharToRespType(char c) {
    switch (c) {
#define XX(name, c) \
    case c:         \
        return ERespType::name;
        FOR_ALL_RESP_TYPES
#undef XX

        default:
            throw RedisError{std::string{"Unknown type byte '"} + c + "'"};
    }
}

inline char RespTypeToChar(ERespType type) {
    switch (type) {
#define XX(name, c)       \
    case ERespType::name: \
        return c;
        FOR_ALL_RESP_TYPES
#undef XX
        default:
            throw RedisError{"Bad ERespType: " + std::to_string(static_cast<int>(type))};
    }
}

inline std::string ToString(ERespType type) {
    switch (type) {
#define XX(name, c)       \
    case ERespType::name: \
        return #name;
        FOR_ALL_RESP_TYPES
#undef XX
        default:
            throw RedisError{"Bad ERespType: " + std::to_string(static_cast<int>(type))};
    }
}

#undef FOR_ALL_RESP_TYPES

}  // namespace redis
