#pragma once

#include "coro/task.h"

#include <span>

namespace redis {
using MutableView = std::span<std::byte>;

class IAsyncReader {
public:
    virtual ~IAsyncReader() = default;

    virtual Task<size_t> Read(MutableView buf) = 0;
    Task<Unit> ReadFull(MutableView buf);
};

}  // namespace redis
