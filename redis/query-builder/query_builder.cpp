#include "query_builder.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace redis {

void QueryBuilder::WriteSimpleString(std::string_view s) {
    request_.append("+" + std::string{s} + "\r\n");
}

void QueryBuilder::WriteError(std::string_view s) {
    request_.append("-" + std::string{s} + "\r\n");
}

void QueryBuilder::WriteInt(int64_t n) {
    request_.append(":" + std::to_string(n) + "\r\n");
}

void QueryBuilder::WriteBulkString(std::string_view s) {
    request_.append("$" + std::to_string(s.size()) + "\r\n" + std::string{s} + "\r\n");
}

void QueryBuilder::WriteNullBulkString() {
    request_.append("$-1\r\n");
}

void QueryBuilder::StartArray(size_t size) {
    request_.append("*" + std::to_string(size) + "\r\n");
}

void QueryBuilder::WriteNullArray() {
    request_.append("*-1\r\n");
}

}  // namespace redis
