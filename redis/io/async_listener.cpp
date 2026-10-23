#include "async_listener.h"
#include "net/event_loop.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <system_error>

namespace redis {

namespace {
struct ListenerAwaiter {
    EventLoop* loop;
    int fd;

    bool await_ready() const noexcept {
        return false;
    }
    void await_suspend(std::coroutine_handle<> h) noexcept {
        loop->Register(fd, Epoll::PortableFlags::READ, h);
    }
    void await_resume() const noexcept {
    }
};
}  // namespace

AsyncSocketListener::AsyncSocketListener(const cactus::SocketAddress& at) : at_(at) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        throw std::system_error(errno, std::system_category(), "socket");
    }
    int flags = fcntl(fd_, F_GETFL, 0);
    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != 0) {
        throw std::system_error(errno, std::system_category(), "fcntl F_SETFL");
    }

    int opt = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(fd_);
        throw std::system_error(errno, std::system_category(), "setsockopt SO_REUSEADDR");
    }

    if (setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1) {
        close(fd_);
        throw std::system_error(errno, std::system_category(), "setsockopt TCP_NODELAY");
    }

    flags = fcntl(fd_, F_GETFD, 0);
    if (flags == -1 || fcntl(fd_, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(fd_);
        throw std::system_error(errno, std::system_category(), "fcntl FD_CLOEXEC");
    }

    sockaddr_storage ss;
    socklen_t slen = at_.GetAddress(&ss);

    if (bind(fd_, reinterpret_cast<const sockaddr*>(&ss), slen) == -1) {
        close(fd_);
        throw std::system_error(errno, std::system_category(), "bind");
    }

    if (listen(fd_, 128) == -1) {
        close(fd_);
        throw std::system_error(errno, std::system_category(), "listen");
    }

    if (getsockname(fd_, reinterpret_cast<sockaddr*>(&ss), &slen) == 0) {
        at_.SetFromSockaddr(reinterpret_cast<sockaddr*>(&ss), slen);
    }
}

AsyncSocketListener::~AsyncSocketListener() {
    Close();
}

void AsyncSocketListener::Close() {
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

Task<std::unique_ptr<Socket>> AsyncSocketListener::Accept() {
    if (fd_ == -1) {
        throw std::system_error(EBADF, std::system_category(), "listener closed");
    }

    EventLoop* loop = EventLoop::active;
    if (!loop) {
        throw std::system_error(ENOTCONN, std::system_category(), "no active event loop");
    }

    while (true) {
        int client_fd = accept(fd_, nullptr, nullptr);

        if (client_fd >= 0) {
            int flags = fcntl(client_fd, F_GETFL, 0);
            if (flags == -1 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                close(client_fd);
                throw std::system_error(errno, std::system_category(), "fcntl F_SETFL O_NONBLOCK");
            }

            int opt = 1;
            setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

            flags = fcntl(client_fd, F_GETFD, 0);
            if (flags == -1 || fcntl(client_fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
                close(client_fd);
                throw std::system_error(errno, std::system_category(), "fcntl FD_CLOEXEC");
            }

            auto socket = std::make_unique<Socket>();
            socket->SetFd(client_fd);
            co_return std::move(socket);
        }

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                co_await ListenerAwaiter{loop, fd_};
                continue;
            }

            throw std::system_error(errno, std::system_category(), "accept");
        }
    }
}

const cactus::SocketAddress& AsyncSocketListener::Address() const {
    return at_;
}

}  // namespace redis
