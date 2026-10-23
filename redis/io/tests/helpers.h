#pragma once

#include <atomic>
#include <coroutine>
#include <unistd.h>
#include <sys/socket.h>

#include <catch2/catch_test_macros.hpp>

namespace redis::tests {
class TestServer {
public:
    TestServer() {
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(listen_fd_ >= 0);
        int flags = fcntl(listen_fd_, F_GETFL, 0);
        REQUIRE(fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK) == 0);

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        REQUIRE(bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) == 0);
        REQUIRE(listen(listen_fd_, 5) == 0);

        socklen_t len = sizeof(addr);
        REQUIRE(getsockname(listen_fd_, (struct sockaddr*)&addr, &len) == 0);
        port_ = ntohs(addr.sin_port);
    }

    ~TestServer() {
        if (listen_fd_ >= 0) {
            close(listen_fd_);
        }
    }

    int Accept() {
        int client_fd = accept(listen_fd_, nullptr, nullptr);
        if (client_fd >= 0) {
            int flags = fcntl(client_fd, F_GETFL, 0);
            fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        }
        return client_fd;
    }

    uint16_t GetPort() const {
        return port_;
    }
    int GetListenFd() const {
        return listen_fd_;
    }

private:
    int listen_fd_ = -1;
    uint16_t port_ = 0;
};
}  // namespace redis::tests
