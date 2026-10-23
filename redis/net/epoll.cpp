#include "epoll.h"

#ifdef __linux__
#include <sys/eventfd.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <fcntl.h>
#else
#error "not supported"
#endif

#include <system_error>
#include <cstring>
#include <vector>

#ifndef EPOLL_CTL_ADD
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_MOD 2
#define EPOLL_CTL_DEL 3
#endif

namespace redis {

Epoll::Epoll() {
#ifdef __linux__
    fd_ = epoll_create1(EPOLL_CLOEXEC);
    const char *message = "epoll_create1";
#elif defined(__APPLE__)
    fd_ = kqueue();
    const char *message = "kqueue";
#else
#error "not supported"
#endif
    if (fd_ == -1) {
        throw std::system_error(errno, std::system_category(), message);
    }
#ifdef __APPLE__
    if (int flags = fcntl(fd_, F_GETFD, 0);
        flags == -1 || fcntl(fd_, F_SETFD, flags | FD_CLOEXEC) == -1) {
        throw std::system_error(errno, std::system_category(), "fcntl");
    }
#endif

#ifdef __linux__
    close_fd_ = eventfd(0, EFD_CLOEXEC);
    if (close_fd_ < 0) {
        throw std::system_error(errno, std::system_category(), "eventfd");
    }

    epoll_event ev;
    ev.events = EPOLLIN;
    if (epoll_ctl(fd_, EPOLL_CTL_ADD, close_fd_, &ev) == -1) {
        close(fd_);
        close(close_fd_);
        throw std::system_error(errno, std::system_category(), "epoll_ctl");
    }
#elif defined(__APPLE__)
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        close(fd_);
        throw std::system_error(errno, std::system_category(), "pipe");
    }

    for (int i = 0; i < 2; i++) {
        if (int flags = fcntl(pipe_fds[i], F_GETFD, 0);
            flags == -1 || fcntl(pipe_fds[i], F_SETFD, flags | FD_CLOEXEC | O_NONBLOCK) == -1) {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            close(fd_);
            throw std::system_error(errno, std::system_category(), "fcntl");
        }
    }

    close_fd_ = pipe_fds[0];
    close_fd_write_ = pipe_fds[1];

    struct kevent ev;
    EV_SET(&ev, close_fd_, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    if (kevent(fd_, &ev, 1, nullptr, 0, nullptr) == -1) {
        close(fd_);
        close(close_fd_);
        close(close_fd_write_);
        throw std::system_error(errno, std::system_category(), "kevent");
    }
#else
#error "not supported"
#endif
}

void Epoll::Close() {
    closed_.test_and_set(std::memory_order_release);
#ifdef __linux__
    uint64_t val = 1;
    if (write(close_fd_, &val, sizeof(val)) < 0) {
        throw std::system_error(errno, std::system_category(), "write");
    }
#elif defined(__APPLE__)
    char buf = 'x';
    if (write(close_fd_write_, &buf, sizeof(buf)) < 0) {
        throw std::system_error(errno, std::system_category(), "write");
    }
#else
#error "not supported"
#endif
}

Epoll::~Epoll() {
    if (fd_ != -1) {
        close(fd_);
    }
    if (close_fd_ != -1) {
        close(close_fd_);
    }
    if (close_fd_write_ != -1) {
        close(close_fd_write_);
    }
}

int Epoll::Update(int fd, uint32_t flags, int cmd) {
#ifdef __linux__
    epoll_event ev{
        .events = flags,
        .data = {.fd = fd},
    };
    return epoll_ctl(fd_, cmd, fd, &ev);
#elif defined(__APPLE__)
    struct kevent changes[2];
    int change_count = 0;

    if (cmd == EPOLL_CTL_DEL) {
        EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        return kevent(fd_, changes, 2, nullptr, 0, nullptr);
    }

    if (cmd == EPOLL_CTL_ADD || cmd == EPOLL_CTL_MOD) {
        if (flags & (PortableFlags::READ | PortableFlags::CLOSE)) {
            EV_SET(&changes[change_count++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        } else if (cmd == EPOLL_CTL_MOD) {
            EV_SET(&changes[change_count++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        }

        if (flags & PortableFlags::WRITE) {
            EV_SET(&changes[change_count++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        } else if (cmd == EPOLL_CTL_MOD) {
            EV_SET(&changes[change_count++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        }

        return kevent(fd_, changes, change_count, nullptr, 0, nullptr);
    }

    errno = EINVAL;
    return -1;
#else
#error "Unsupported platform"
#endif
}

int Epoll::Register(int fd, uint32_t flags) {
    if (registered_fds_.find(fd) != registered_fds_.end()) {
        errno = EEXIST;
        return -1;
    }
    auto ret = Update(fd, flags, EPOLL_CTL_ADD);
    if (ret != -1) {
        registered_fds_.insert(fd);
    }
    return ret;
}

int Epoll::Modify(int fd, uint32_t flags) {
    return Update(fd, flags, EPOLL_CTL_MOD);
}

int Epoll::Unregister(int fd) {
    if (auto it = registered_fds_.find(fd); it != registered_fds_.end()) {
        registered_fds_.erase(it);
    }

    auto ret = Update(fd, 0, EPOLL_CTL_DEL);
    if (ret == -1 && errno == ENOENT) {
        return 0;
    }

    return ret;
}

int Epoll::Poll(int timeout, MutableView buf) {
    constexpr int kEpollBatchSize = 64;
    constexpr size_t kEventSize = sizeof(uint32_t) + sizeof(int);

    if (buf.size() < kEventSize) {
        errno = EINVAL;
        return -1;
    }

    int max_events = std::min<int>(kEpollBatchSize, buf.size() / kEventSize);

#ifdef __linux__
    std::vector<epoll_event> events(max_events);
    int nevents = epoll_wait(fd_, events.data(), max_events, timeout);
    const char *message = "epoll_wait";
#elif defined(__APPLE__)
    std::vector<struct kevent> events(max_events);

    struct timespec ts;
    struct timespec *ts_ptr = nullptr;
    if (timeout >= 0) {
        ts.tv_sec = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000;
        ts_ptr = &ts;
    }
    int nevents = kevent(fd_, nullptr, 0, events.data(), max_events, ts_ptr);
    const char *message = "kevent";
#else
#error "not supported"
#endif

    if (nevents == -1) {
        if (errno == EINTR) {
            return 0;
        }

        throw std::system_error(errno, std::system_category(), message);
    }

    if (closed_.test(std::memory_order_acquire)) {
        return -1;
    }

    size_t offset = 0;
    for (int i = 0; i < nevents; ++i) {
#ifdef __linux__
        uint32_t flags = events[i].events;
        int fd = events[i].data.fd;
#elif defined(__APPLE__)
        int fd = events[i].ident;
        if (fd == close_fd_) {
            char buf[64];
            ssize_t ret = read(close_fd_, buf, sizeof(buf));
            (void)ret;
            continue;
        }
        uint32_t flags{};
        if (events[i].filter == EVFILT_READ) {
            flags |= PortableFlags::READ;
        }
        if (events[i].filter == EVFILT_WRITE) {
            flags |= PortableFlags::WRITE;
        }
        if (events[i].flags & EV_EOF) {
            flags |= PortableFlags::CLOSE;
        }
#else
#error "not supported"
#endif
        std::memcpy(buf.data() + offset, &flags, sizeof(flags));
        offset += sizeof(flags);
        std::memcpy(buf.data() + offset, &fd, sizeof(fd));
        offset += sizeof(fd);
    }

    return nevents;
}

}  // namespace redis
