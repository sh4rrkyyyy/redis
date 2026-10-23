#include "socket.h"
#include "helpers.h"

#include "coro/common.h"

#include <thread>

using namespace redis;         // NOLINT
using namespace redis::tests;  // NOLINT

TEST_CASE("Ctor/Dtor") {
    SECTION("Default") {
        REQUIRE_NOTHROW(Socket());
    }

    SECTION("Multiple") {
        Socket sock1;
        Socket sock2;
        REQUIRE(sock1.GetFd() >= 0);
        REQUIRE(sock2.GetFd() >= 0);
        REQUIRE(sock1.GetFd() != sock2.GetFd());
    }

    SECTION("Close") {
        Socket sock;
        int fd = sock.GetFd();
        REQUIRE(fd >= 0);
        sock.Close();
        REQUIRE(sock.GetFd() == -1);
    }

    SECTION("Double close") {
        Socket sock;
        sock.Close();
        sock.Close();
        SUCCEED();
    }
}

TEST_CASE("Connect") {
    TestServer server;
    EventLoop loop;

    SECTION("Valid") {
        Socket sock;

        bool connected = false;
        std::exception_ptr error;

        auto connect_task = [&]() -> Task<Unit> {
            try {
                cactus::SocketAddress addr("127.0.0.1", server.GetPort());
                co_await sock.Connect(addr);
                connected = true;
                co_return Unit{};
            } catch (...) {
                error = std::current_exception();
            }
            co_return Unit{};
        };

        auto connect_handle = connect_task();
        connect_handle.Resume();

        std::thread loop_thread([&]() {
            loop.SetActive();

            while (!connected && !error) {
                loop.RunOnce(10);
            }

            loop.Stop();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int client_fd = server.Accept();
        REQUIRE(client_fd >= 0);

        loop_thread.join();

        CHECK(connected);
        CHECK_FALSE(error);

        close(client_fd);
    }
}

TEST_CASE("Read&Write") {
    EventLoop loop;
    int server_fds[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, server_fds) == 0);
    for (int i = 0; i < 2; i++) {
        int flags = fcntl(server_fds[i], F_GETFL, 0);
        REQUIRE(flags != -1);
        REQUIRE(fcntl(server_fds[i], F_SETFL, flags | O_NONBLOCK) != -1);
    }

    Socket sock;
    sock.SetFd(server_fds[0]);

    SECTION("Write then read") {
        const std::string test_data = "Hello, World!";

        auto write_task = [&]() -> Task<Unit> {
            ConstView view(reinterpret_cast<const std::byte*>(test_data.data()), test_data.size());
            co_await sock.Write(view);
            co_return Unit{};
        };

        auto read_task = [&]() -> Task<size_t> {
            std::array<std::byte, 1024> buffer;
            MutableView view(buffer);
            co_return co_await sock.Read(view);
        };

        SyncWait(write_task());

        char server_buf[1024];
        ssize_t n = read(server_fds[1], server_buf, sizeof(server_buf));
        REQUIRE(n > 0);
        server_buf[n] = '\0';
        CHECK(std::string(server_buf) == test_data);

        const std::string response = "hello <username>";
        n = write(server_fds[1], response.data(), response.size());
        REQUIRE(n == static_cast<ssize_t>(response.size()));

        size_t read_size = SyncWait(read_task());
        CHECK(read_size == response.size());
    }

    SECTION("Write EOF") {
        sock.Close();

        std::string data = "test";
        ConstView view(reinterpret_cast<const std::byte*>(data.data()), data.size());

        REQUIRE_THROWS_AS(SyncWait([&] -> Task<Unit> {
                              co_await sock.Write(view);
                              co_return Unit{};
                          }()),
                          std::system_error);
    }

    close(server_fds[1]);
}

TEST_CASE("Bind") {
    EventLoop loop;

    SECTION("Bind to port") {
        Socket sock;
        cactus::SocketAddress addr("127.0.0.1", 0);

        SyncWait([&] -> Task<Unit> {
            co_await sock.Bind(addr);
            co_return Unit{};
        }());

        struct sockaddr_in bound_addr;
        socklen_t len = sizeof(bound_addr);
        REQUIRE(getsockname(sock.GetFd(), (struct sockaddr*)&bound_addr, &len) == 0);
        CHECK(bound_addr.sin_family == AF_INET);
        CHECK(bound_addr.sin_addr.s_addr == ntohl(INADDR_LOOPBACK));

        sock.Close();
    }
}
