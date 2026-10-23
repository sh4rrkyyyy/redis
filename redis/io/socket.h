#pragma once

#include "net/event_loop.h"
#include "coro/task.h"
#include "async_reader.h"
#include "async_writer.h"

#include <cactus/net/address.h>

#include <atomic>
#include <coroutine>
#include <system_error>
#include <span>

namespace redis {
using ConstView = std::span<const std::byte>;
using MutableView = std::span<std::byte>;

class Socket : public IAsyncReader, public IAsyncWriter {
public:
    Socket();
    ~Socket() override;

    Task<size_t> Read(MutableView buf) override;
    Task<Unit> Write(ConstView buf) override;

    Task<Unit> Connect(const cactus::SocketAddress& address);
    Task<Unit> Bind(const cactus::SocketAddress& address);
    void Close() noexcept;

    [[nodiscard]] int GetFd() const noexcept;
    void SetFd(int fd) noexcept;

private:
    int fd_ = -1;
};

}  // namespace redis
