#include "socket.h"

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <system_error>

namespace redis {

namespace {
struct IOAwaiter {
    EventLoop* loop;
    int fd;
    Epoll::PortableFlags event;

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        loop->Register(fd, event, h);
    }

    void await_resume() noexcept {
    }
};
}  // namespace

Socket::Socket() {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        throw std::system_error(errno, std::system_category(), "socket");
    }

    int flags = fcntl(fd_, F_GETFL, 0);
    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != 0) {
        throw std::system_error(errno, std::system_category(), "fcntl F_SETFL");
    }

    int flag = 1;
    if (setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == -1) {
        close(fd_);
        throw std::system_error(errno, std::system_category(), "setsockopt TCP_NODELAY");
    }

    flags = fcntl(fd_, F_GETFD, 0);
    if (flags == -1 || fcntl(fd_, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(fd_);
        throw std::system_error(errno, std::system_category(), "fcntl FD_CLOEXEC");
    }
}

Socket::~Socket() {
    Close();
}

int Socket::GetFd() const noexcept {
    return fd_;
}

Task<size_t> Socket::Read(MutableView buf) {
    EventLoop* loop = EventLoop::active;
    if (!loop) {
        throw std::system_error(ENOTCONN, std::system_category(), "no active event loop");
    }

    if (fd_ == -1) {
        throw std::system_error(ENOTCONN, std::system_category(), "socket not connected");
    }

    while (true) {
        ssize_t n = recv(fd_, buf.data(), buf.size(), 0);
        if (n >= 0) {
            co_return static_cast<size_t>(n);
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            co_await IOAwaiter{loop, fd_, Epoll::PortableFlags::READ};
            continue;
        }

        throw std::system_error(errno, std::system_category(), "recv");
    }
}

Task<Unit> Socket::Write(ConstView buf) {
    EventLoop* loop = EventLoop::active;
    if (!loop) {
        throw std::system_error(ENOTCONN, std::system_category(), "no active event loop");
    }

    if (fd_ == -1) {
        throw std::system_error(ENOTCONN, std::system_category(), "socket not connected");
    }

    size_t total_written = 0;
    while (total_written < buf.size()) {
        ssize_t n = send(fd_, buf.data() + total_written, buf.size() - total_written, 0);
        if (n > 0) {
            total_written += n;
            continue;
        }

        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                co_await IOAwaiter{loop, fd_, Epoll::PortableFlags::WRITE};
                continue;
            }

            throw std::system_error(errno, std::system_category(), "send");
        }
    }

    co_return Unit{};
}

Task<Unit> Socket::Connect(const cactus::SocketAddress& address) {
    if (fd_ == -1) {
        throw std::system_error(EBADF, std::system_category(), "invalid socket");
    }

    struct sockaddr_storage addr;
    auto len = address.GetAddress(&addr);

    int ret = connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), len);
    if (ret == 0) {
        co_return Unit{};
    }

    if (ret == -1 && (errno == EINPROGRESS || errno == EAGAIN)) {
        EventLoop* loop = EventLoop::active;
        if (!loop) {
            throw std::system_error(ENOTCONN, std::system_category(), "no active event loop");
        }

        co_await IOAwaiter{loop, fd_, Epoll::PortableFlags::WRITE};

        int so_error;
        socklen_t len = sizeof(so_error);
        if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &so_error, &len) == -1) {
            throw std::system_error(errno, std::system_category(), "getsockopt");
        }

        if (so_error != 0) {
            throw std::system_error(so_error, std::system_category(), "connect");
        }

        co_return Unit{};
    }

    throw std::system_error(errno, std::system_category(), "connect");
}

Task<Unit> Socket::Bind(const cactus::SocketAddress& address) {
    if (fd_ == -1) {
        throw std::system_error(EBADF, std::system_category(), "invalid socket");
    }

    struct sockaddr_storage addr;
    auto len = address.GetAddress(&addr);

    if (bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), len) == -1) {
        throw std::system_error(errno, std::system_category(), "bind");
    }

    co_return Unit{};
}

void Socket::Close() noexcept {
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

void Socket::SetFd(int fd) noexcept {
    Close();
    fd_ = fd;
}

}  // namespace redis
