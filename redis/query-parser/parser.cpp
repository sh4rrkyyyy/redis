#include "parser.h"
#include <ranges>
#include <string>
#include "error.h"
#include "resp_types.h"

namespace redis {
std::pair<RespValue, size_t> ParseImpl(std::span<std::byte const> v);
}

namespace {

constexpr std::array kCrlf{std::byte{'\r'}, std::byte{'\n'}};

auto MakeStrFrom(std::span<std::byte const> data) {
    auto crlf = std::ranges::search(data, kCrlf);
    if (crlf.empty()) {
        throw redis::RedisError{"CRLF not found"};
    }
    for (auto it = data.begin() + 1; it != crlf.begin(); ++it) {
        if (*it == std::byte{'\r'} || *it == std::byte{'\n'}) {
            throw redis::RedisError{"Unexpected CR or LF"};
        }
    }
    return std::string{reinterpret_cast<const char*>(data.data() + 1),
                       reinterpret_cast<const char*>(std::to_address(crlf.begin()))};
}

std::pair<int64_t, size_t> MakeIntFrom(std::span<std::byte const> data) {
    auto s = MakeStrFrom(data);
    auto sz = s.size();

    size_t pos = 0;
    int64_t value = std::stoll(s, &pos);

    if (pos != s.size()) {
        throw redis::RedisError{"Invalid integer"};
    }

    return {value, 3 + sz};
}

std::pair<std::string, size_t> MakeBulkStrFrom(std::span<std::byte const> data) {
    auto sz_str = MakeStrFrom(data);
    if (sz_str.front() == '-') {
        throw redis::RedisError{"Invalid size"};
    }
    size_t sz = std::stol(sz_str);
    auto st_pos = 1 + sz_str.size() + 2;

    if (data.size() < st_pos + sz + 2 || data[st_pos + sz] != std::byte{'\r'} ||
        data[st_pos + sz + 1] != std::byte{'\n'}) {
        throw redis::RedisError{"Invalid bulk string"};
    }

    std::string value{reinterpret_cast<const char*>(data.data() + st_pos),
                      reinterpret_cast<const char*>(data.data() + st_pos + sz)};

    return {std::move(value), st_pos + sz};
}

std::pair<std::vector<std::unique_ptr<redis::RespValue>>, size_t> MakeArrayFrom(
    std::span<std::byte const> data) {
    auto sz_str = MakeStrFrom(data);
    size_t sz = std::stol(sz_str);
    auto pos = 1 + sz_str.size() + 2;
    std::vector<std::unique_ptr<redis::RespValue>> res;
    for (size_t i = 0; i < sz; ++i) {
        auto [resp_value, new_pos] = redis::ParseImpl(data.subspan(pos));
        pos += new_pos;
        res.push_back(std::make_unique<redis::RespValue>(std::move(resp_value)));
    }
    return {std::move(res), pos};
}

}  // namespace

namespace redis {

std::pair<RespValue, size_t> ParseImpl(std::span<std::byte const> v) {
    auto c = v[0];
    switch (c) {
        case std::byte{'+'}: {
            std::string value = MakeStrFrom(v);
            size_t sz = value.size();
            return {RespValue{
                        .type = ERespType::SimpleString, .intVal = {}, .strVal = std::move(value)},
                    3 + sz};
        }
        case std::byte{'-'}: {
            std::string value = MakeStrFrom(v);
            size_t sz = value.size();
            return {RespValue{.type = ERespType::Error, .intVal = {}, .strVal = std::move(value)},
                    3 + sz};
        }
        case std::byte{':'}: {
            auto [value, sz] = MakeIntFrom(v);
            return {RespValue{.type = ERespType::Int, .intVal = value}, sz};
        }
        case std::byte{'$'}: {
            if (v[1] == std::byte{'-'} && v[2] == std::byte{'1'}) {
                return {RespValue{.type = ERespType::Empty}, 5};
            }
            auto [value, sz] = MakeBulkStrFrom(v);
            return {
                RespValue{.type = ERespType::BulkString, .intVal = {}, .strVal = std::move(value)},
                2 + sz};
        }
        case std::byte{'*'}: {
            if (v[1] == std::byte{'-'} && v[2] == std::byte{'1'}) {
                return {RespValue{.type = ERespType::Empty}, 5};
            }
            auto [value, sz] = MakeArrayFrom(v);
            return {
                RespValue{
                    .type = ERespType::Array, .intVal = {}, .strVal = {}, .arr = std::move(value)},
                sz};
        }
        default:
            throw RedisError{"Incorrect type of operation"};
    }
}

RespValue QueryParser::Parse(ConstView& v) {
    return ParseImpl(v).first;
}

}  // namespace redis
