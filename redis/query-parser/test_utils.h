#pragma once

#include "parser.h"
#include "io/async_reader.h"
#include "coro/common.h"

#include <catch2/catch_test_macros.hpp>

#include "utils.h"

#include <deque>

namespace redis::test {
using ConstView = std::span<const std::byte>;
using MutableView = std::span<std::byte>;

class IBufferedAsyncReader : public IAsyncReader {
public:
    virtual ConstView ReadNext() = 0;
    virtual void ReadBackUp(size_t size) = 0;

    Task<size_t> Read(MutableView buf) override {
        size_t bytes_read = 0;

        auto chunk = ReadNext();
        if (chunk.empty()) {
            co_return bytes_read;
        }

        int64_t copy_size = std::min(buf.size(), chunk.size());
        std::ranges::copy(chunk.first(copy_size), buf.begin());

        buf = buf.subspan(copy_size);
        chunk = chunk.subspan(copy_size);
        bytes_read += copy_size;

        if (!chunk.empty()) {
            ReadBackUp(chunk.size());
        }

        co_return bytes_read;
    }
};

class ViewAsyncReader : public IBufferedAsyncReader {
public:
    explicit ViewAsyncReader(ConstView buf) : buf_(buf) {
    }

    ConstView ReadNext() override {
        auto result = buf_;
        result = result.subspan(pos_);
        pos_ = buf_.size();
        return result;
    }

    virtual void ReadBackUp(size_t size) override {
        assert(size <= pos_);
        pos_ -= size;
    }

private:
    ConstView buf_;
    size_t pos_ = 0;
};

struct SyncReader {
    IAsyncReader* conn;
    QueryParser parser;

    auto Parse() {
        char buffer[1024]{};
        auto n = SyncWait([this, &buffer] -> Task<size_t> {
            auto view = std::as_writable_bytes(
                std::span{std::ranges::data(buffer), std::ranges::size(buffer)});
            auto n = co_await conn->Read(view);
            co_return n;
        }());

        REQUIRE(n);

        auto v = ConstView(reinterpret_cast<const std::byte*>(buffer), n);

        return parser.Parse(v);
    }
};

class ReaderTest {
public:
    static ViewAsyncReader MakeViewReader(const char* data) {
        auto view = std::as_bytes(std::span{data, std::strlen(data)});
        return ViewAsyncReader{view};
    }

    SyncReader MakeParser(const char* data) {
        auto& view = views_.emplace_back(MakeViewReader(data));
        return SyncReader{&view, {}};
    }

private:
    std::deque<ViewAsyncReader> views_;

#ifdef __linux__
    // Protection from OOM
    MemoryGuard guard_ = MakeMemoryGuard(10'000'000);
#endif
};

}  // namespace redis::test
