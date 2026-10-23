#pragma once

#include "coro/task.h"
#include "socket.h"

#include <cactus/net/address.h>

#include <memory>

namespace redis {
class AsyncSocketListener {
public:
    AsyncSocketListener(const AsyncSocketListener&) = delete;
    AsyncSocketListener& operator=(const AsyncSocketListener&) = delete;
    AsyncSocketListener(AsyncSocketListener&&) = delete;
    AsyncSocketListener& operator=(AsyncSocketListener&&) = delete;

    AsyncSocketListener(const cactus::SocketAddress& at);
    void Close();

    Task<std::unique_ptr<Socket>> Accept();
    const cactus::SocketAddress& Address() const;
    ~AsyncSocketListener();

private:
    int fd_ = -1;
    cactus::SocketAddress at_;
};

}  // namespace redis
