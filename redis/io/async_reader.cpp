#include "async_reader.h"
#include "error.h"

#include <cassert>
#include <string>

namespace redis {
Task<Unit> IAsyncReader::ReadFull(MutableView buf) {
    if (buf.empty()) {
        co_return Unit{};
    }

    while (!buf.empty()) {
        auto n = co_await Read(buf);
        if (n == 0) {
            break;
        }

        buf = buf.subspan(n);
    }

    if (!buf.empty()) {
        throw RedisError("EOF");
    }

    co_return Unit{};
}

}  // namespace redis
