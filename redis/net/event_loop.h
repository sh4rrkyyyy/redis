#pragma once

#include "epoll.h"

#include <atomic>
#include <coroutine>
#include <unordered_map>
#include <vector>

namespace redis {

class EventLoop {
public:
    static thread_local EventLoop* active;
    using CoroutineHandle = std::coroutine_handle<>;

    EventLoop();
    ~EventLoop();

    void Stop();
    void Run();
    void RunOnce(int timeout_ms);
    int Register(int fd, uint32_t events, CoroutineHandle handle);
    void Unregister(int fd) noexcept;
    void SetActive();

private:
    static constexpr size_t kEventSize = sizeof(uint32_t) + sizeof(int);
    static constexpr size_t kDefaultEventBatch = 64;
    static constexpr int kDefaultTimeoutMs = 100;

    std::atomic_flag stopped_;

    Epoll epoll_;
    int wakeup_fd_{-1}, wakeup_fd_write_{-1};

    std::vector<std::byte> event_buffer_;

    struct IOWaiter {
        uint32_t events;
        CoroutineHandle handle;
    };
    std::unordered_map<int, IOWaiter> io_waiters_;

    void Wake() noexcept;
    void DrainWakeupFd() noexcept;
};

}  // namespace redis
