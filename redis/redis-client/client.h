#pragma once

#include "query-builder/query_builder.h"
#include "io/socket.h"
#include "coro/task.h"

#include <string>
#include <string_view>
#include <optional>

#include <cactus/net/address.h>

namespace redis {

class Client {
public:
    explicit Client(const cactus::SocketAddress& server_address) : addr_(server_address) {
    }
    ~Client() = default;

    Task<Unit> Connect();
    Task<Unit> Disconnect();

    Task<std::optional<std::string>> Get(std::string_view key);
    Task<Unit> Set(std::string_view key, std::string_view value);
    Task<int64_t> Increment(std::string_view key);
    Task<int64_t> Decrement(std::string_view key);

private:
    std::optional<std::string> ParseGetRsp(std::span<const std::byte> data);
    Unit ParseSetRsp(std::span<const std::byte> data);
    int64_t ParseIncrDecrRsp(std::span<const std::byte> data);
    cactus::SocketAddress addr_;
    Socket socket_;
};

}  // namespace redis
