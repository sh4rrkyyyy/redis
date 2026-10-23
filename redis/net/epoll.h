#pragma once

#ifdef __linux__
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#elif defined(__APPLE__)
#include <sys/event.h>
#include <unistd.h>
#include <fcntl.h>
#else
#error "not supported"
#endif

#include <atomic>
#include <span>
#include <unordered_set>

namespace redis {
using MutableView = std::span<std::byte>;

class Epoll {

#ifdef __linux__
    using event_type = epoll_event;  // NOLINT
#elif defined(__APPLE__)
    using event_type = struct kevent;  // NOLINT
#else
#error "not supported"
#endif

#ifdef __linux__
#define READ_EVENT EPOLLIN
#define WRITE_EVENT EPOLLOUT
#define EOF_FLAG EPOLLERR
#elif defined(__APPLE__)
#define READ_EVENT 1
#define WRITE_EVENT 2
#define EOF_FLAG EV_EOF
#else
#error "not supported"
#endif

public:
    enum PortableFlags : uint32_t {
        READ = READ_EVENT,
        WRITE = WRITE_EVENT,
        CLOSE = EOF_FLAG,
    };

    Epoll();
    ~Epoll();

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    [[nodiscard]] int Register(int fd, uint32_t flags);
    [[nodiscard]] int Modify(int fd, uint32_t flags);
    int Unregister(int fd);

    void Close();
    int Poll(int timeout, MutableView buf);

private:
    [[nodiscard]] int Update(int fd, uint32_t flags, int cmd);

    std::atomic_flag closed_;
    int fd_{-1}, close_fd_{-1}, close_fd_write_{-1};
    std::unordered_set<int> registered_fds_;
};

}  // namespace redis
