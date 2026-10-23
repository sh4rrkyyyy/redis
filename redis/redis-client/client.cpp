#include "client.h"
#include "coro/task.h"
#include "error.h"
#include "query-builder/query_builder.h"
#include "query-parser/parser.h"
#include "resp_types.h"

namespace redis {

Task<Unit> Client::Connect() {
    co_return co_await socket_.Connect(addr_);
}

Task<Unit> Client::Disconnect() {
    socket_.Close();
    co_return Unit{};
}

std::optional<std::string> Client::ParseGetRsp(std::span<const std::byte> data) {
    QueryParser qp;
    auto resp_value = qp.Parse(data);
    switch (resp_value.type) {
        case ERespType::SimpleString:
            return resp_value.AsString();
        case ERespType::Error:
            throw RedisError{"Error"};
        case ERespType::BulkString:
            return resp_value.AsString();
        case ERespType::Int:
            throw RedisError{"Int"};
        case ERespType::Array:
            throw RedisError{"Array"};
        case ERespType::Empty:
            return {};
    }
    return {};
}

Unit Client::ParseSetRsp(std::span<const std::byte> data) {
    QueryParser qp;
    auto resp_value = qp.Parse(data);
    switch (resp_value.type) {
        case ERespType::BulkString:
            if (resp_value.AsString() == "OK") {
                return Unit{};
            }
            [[fallthrough]];
        default:
            throw RedisError{"Bad response"};
    }
}

int64_t Client::ParseIncrDecrRsp(std::span<const std::byte> data) {
    QueryParser qp;
    auto resp_value = qp.Parse(data);
    switch (resp_value.type) {
        case ERespType::Int:
            return resp_value.AsInteger();
        default:
            throw RedisError{"Bad response"};
    }
}

Task<std::optional<std::string>> Client::Get(std::string_view key) {
    QueryBuilder qb;
    qb.StartArray(2);
    qb.WriteBulkString("GET");
    qb.WriteBulkString(key);
    auto req = qb.Request();
    co_await socket_.Write({reinterpret_cast<const std::byte*>(req.data()), req.size()});
    std::array<std::byte, 256> buf;
    auto sz = co_await socket_.Read(buf);
    std::span<const std::byte> view{buf.data(), sz};
    co_return ParseGetRsp(view);
}

Task<Unit> Client::Set(std::string_view key, std::string_view value) {
    QueryBuilder qb;
    qb.StartArray(3);
    qb.WriteBulkString("SET");
    qb.WriteBulkString(key);
    qb.WriteBulkString(value);
    auto req = qb.Request();
    co_await socket_.Write({reinterpret_cast<const std::byte*>(req.data()), req.size()});
    std::array<std::byte, 256> buf;
    auto sz = co_await socket_.Read(buf);
    std::span<const std::byte> view{buf.data(), sz};
    co_return ParseSetRsp(view);
}

Task<int64_t> Client::Increment(std::string_view key) {
    QueryBuilder qb;
    qb.StartArray(2);
    qb.WriteBulkString("INCR");
    qb.WriteBulkString(key);
    auto req = qb.Request();
    co_await socket_.Write({reinterpret_cast<const std::byte*>(req.data()), req.size()});
    std::array<std::byte, 256> buf;
    auto sz = co_await socket_.Read(buf);
    std::span<const std::byte> view{buf.data(), sz};
    co_return ParseIncrDecrRsp(view);
}

Task<int64_t> Client::Decrement(std::string_view key) {
    QueryBuilder qb;
    qb.StartArray(2);
    qb.WriteBulkString("DECR");
    qb.WriteBulkString(key);

    auto req = qb.Request();
    co_await socket_.Write({reinterpret_cast<const std::byte*>(req.data()), req.size()});
    std::array<std::byte, 256> buf;
    auto sz = co_await socket_.Read(buf);
    std::span<const std::byte> view{buf.data(), sz};
    co_return ParseIncrDecrRsp(view);
}

}  // namespace redis
