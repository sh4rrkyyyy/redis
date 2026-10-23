#include "event_loop.h"

#ifdef __linux__
#include <sys/eventfd.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <fcntl.h>
#else
#error "not supported"
#endif

#include <system_error>
#include <cstring>
#include <cassert>

namespace redis {

thread_local EventLoop* EventLoop::active = nullptr;

EventLoop::EventLoop() : event_buffer_(kDefaultEventBatch * kEventSize) {

#ifdef __linux__
    wakeup_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wakeup_fd_ == -1) {
        throw std::system_error(errno, std::system_category(), "eventfd");
    }

    if (epoll_.Register(wakeup_fd_, Epoll::PortableFlags::READ) != 0) {
        close(wakeup_fd_);
        throw std::system_error(errno, std::system_category(), "epoll register");
    }
#elif defined(__APPLE__)
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        throw std::system_error(errno, std::system_category(), "pipe");
    }

    for (int i = 0; i < 2; i++) {
        int flags = fcntl(pipe_fds[i], F_GETFL, 0);
        if (flags == -1 || fcntl(pipe_fds[i], F_SETFL, flags | O_NONBLOCK) == -1) {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            throw std::system_error(errno, std::system_category(), "fcntl");
        }

        flags = fcntl(pipe_fds[i], F_GETFD, 0);
        if (flags == -1 || fcntl(pipe_fds[i], F_SETFD, flags | FD_CLOEXEC) == -1) {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            throw std::system_error(errno, std::system_category(), "fcntl");
        }
    }

    wakeup_fd_ = pipe_fds[0];
    wakeup_fd_write_ = pipe_fds[1];

    if (epoll_.Register(wakeup_fd_, Epoll::PortableFlags::READ) != 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        throw std::system_error(errno, std::system_category(), "epoll register");
    }
#else
#error "not supported"
#endif

    active = this;
}

EventLoop::~EventLoop() {
    Stop();

    if (wakeup_fd_ != -1) {
        close(wakeup_fd_);
        wakeup_fd_ = -1;
    }

    if (wakeup_fd_write_ != -1) {
        close(wakeup_fd_write_);
        wakeup_fd_write_ = -1;
    }

    if (active == this) {
        active = nullptr;
    }
}

void EventLoop::Stop() {
    if (!stopped_.test_and_set(std::memory_order_release)) {
        Wake();
    }
}

void EventLoop::Wake() noexcept {
#ifdef __linux__
    uint64_t val = 1;
    ssize_t ret = write(wakeup_fd_, &val, sizeof(val));
    (void)ret;
#elif defined(__APPLE__)
    if (wakeup_fd_write_ != -1) {
        char c = 'x';
        ssize_t ret = write(wakeup_fd_write_, &c, 1);
        (void)ret;
    }
#endif
}

void EventLoop::DrainWakeupFd() noexcept {
#ifdef __linux__
    uint64_t val;
    while (read(wakeup_fd_, &val, sizeof(val)) > 0) {
    }
#elif defined(__APPLE__)
    char c;
    while (read(wakeup_fd_, &c, 1) > 0) {
    }
#endif
}

int EventLoop::Register(int fd, uint32_t events, CoroutineHandle handle) {
    assert(active == this);

    if (!handle || handle.done()) {
        return -1;
    }

    auto it = io_waiters_.find(fd);
    if (it != io_waiters_.end()) {
        if (it->second.events != events) {
            if (epoll_.Modify(fd, events) == 0) {
                it->second.events = events;
            } else {
                return -1;
            }
        }
        it->second.handle = handle;
    } else {
        if (epoll_.Register(fd, events) == 0) {
            io_waiters_[fd] = {events, handle};
        } else {
            return -1;
        }
    }

    return 0;
}

void EventLoop::Unregister(int fd) noexcept {
    assert(active == this);

    if (auto it = io_waiters_.find(fd); it != io_waiters_.end()) {
        epoll_.Unregister(fd);
        io_waiters_.erase(it);
    }
}

void EventLoop::Run() {
    assert(active == this);

    while (!stopped_.test(std::memory_order_acquire)) {
        RunOnce(kDefaultTimeoutMs);
    }
}

void EventLoop::RunOnce(int timeout_ms) {
    int nevents = epoll_.Poll(timeout_ms, MutableView(event_buffer_));

    if (nevents < 0) {
        if (errno != EINTR) {
            throw std::system_error(errno, std::system_category(), "epoll poll");
        }
        return;
    }

    if (nevents == 0) {
        return;
    }

    size_t offset = 0;
    std::vector<CoroutineHandle> handles_to_resume;

    for (int i = 0; i < nevents; ++i) {
        if (offset + kEventSize > event_buffer_.size()) {
            break;
        }

        uint32_t events;
        int fd;

        std::memcpy(&events, event_buffer_.data() + offset, sizeof(events));
        offset += sizeof(events);
        std::memcpy(&fd, event_buffer_.data() + offset, sizeof(fd));
        offset += sizeof(fd);

        if (fd == wakeup_fd_) {
            DrainWakeupFd();
            continue;
        }

        if (auto it = io_waiters_.find(fd); it != io_waiters_.end()) {
            handles_to_resume.push_back(it->second.handle);
        }
    }

    for (auto& handle : handles_to_resume) {
        if (handle && !handle.done()) {
            handle.resume();
        }
    }
}

void EventLoop::SetActive() {
    active = this;
}

}  // namespace redis
