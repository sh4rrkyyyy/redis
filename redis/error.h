#pragma once

#include <stdexcept>

namespace redis {

class RedisError : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

}  // namespace redis
