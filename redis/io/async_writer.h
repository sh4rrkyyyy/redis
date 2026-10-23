#pragma once

#include "coro/task.h"

#include <span>

namespace redis {
class IAsyncWriter {
public:
    using ConstView = std::span<const std::byte>;
    virtual ~IAsyncWriter() = default;
    virtual Task<Unit> Write(ConstView buf) = 0;
};

}  // namespace redis
