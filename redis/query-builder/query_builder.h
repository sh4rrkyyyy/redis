#pragma once

#include "resp_types.h"

#include <string_view>

namespace redis {

class QueryBuilder {
public:
    std::string_view Request() const {
        return request_;
    }

    void WriteSimpleString(std::string_view s);
    void WriteError(std::string_view s);
    void WriteInt(int64_t n);
    void WriteBulkString(std::string_view s);
    void WriteNullBulkString();
    void StartArray(size_t size);
    void WriteNullArray();
    void WriteArrayInts(auto&& range) {
        size_t sz{0};
        for ([[maybe_unused]] auto _ : range) {
            ++sz;
        }
        StartArray(sz);
        for (auto x : range) {
            WriteInt(x);
        }
    }

private:
    std::string request_;
};

}  // namespace redis
