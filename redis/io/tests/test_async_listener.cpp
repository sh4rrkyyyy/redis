#include "async_listener.h"
#include "helpers.h"
#include "coro/common.h"

#include <thread>

using namespace redis;         // NOLINT
using namespace redis::tests;  // NOLINT

TEST_CASE("Ctor/Dtor") {
    SECTION("Random port") {
        cactus::SocketAddress addr("127.0.0.1", 0);
        AsyncSocketListener listener(addr);

        CHECK(listener.Address().GetPort() > 0);
        CHECK(listener.Address().GetIp() == addr.GetIp());
    }

    SECTION("Specific port") {
        cactus::SocketAddress addr("127.0.0.1", 12345);
        AsyncSocketListener listener(addr);

        CHECK(listener.Address().GetPort() == 12345);
        CHECK(listener.Address().GetIp() == addr.GetIp());
    }

    SECTION("Invalid") {
        cactus::SocketAddress addr("0.0.0.0", 0);
        REQUIRE_NOTHROW(AsyncSocketListener(addr));
    }

    SECTION("Close 2x") {
        cactus::SocketAddress addr("127.0.0.1", 0);
        AsyncSocketListener listener(addr);

        listener.Close();
        listener.Close();
    }
}

TEST_CASE("Accept") {
    cactus::SocketAddress addr("127.0.0.1", 0);
    AsyncSocketListener listener(addr);
    EventLoop loop;

    uint16_t port = listener.Address().GetPort();

    std::thread client_thread([port]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(sock >= 0);

        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        int ret =
            connect(sock, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
        REQUIRE(ret == 0);

        const char* msg = "Hello from client";
        send(sock, msg, strlen(msg), 0);

        close(sock);
    });

    bool accepted = false;

    auto accept_task = [&]() -> Task<Unit> {
        auto client_socket = std::move(co_await listener.Accept());
        REQUIRE(client_socket != nullptr);
        REQUIRE(client_socket->GetFd() >= 0);

        std::array<std::byte, 1024> buffer;
        MutableView view(buffer);
        size_t n = co_await client_socket->Read(view);

        std::string received(reinterpret_cast<const char*>(buffer.data()), n);
        CHECK(received == "Hello from client");

        accepted = true;

        co_return Unit{};
    };

    auto accept_handle = accept_task();
    accept_handle.Resume();

    std::thread loop_thread([&]() {
        loop.SetActive();

        while (!accepted) {
            loop.RunOnce(10);
        }

        loop.Stop();
    });

    client_thread.join();
    loop_thread.join();
}

TEST_CASE("Accept after close") {
    cactus::SocketAddress addr("127.0.0.1", 0);
    AsyncSocketListener listener(addr);
    EventLoop loop;

    listener.Close();

    auto accept_task = [&]() -> Task<Unit> {
        co_await listener.Accept();
        co_return Unit{};
    };

    REQUIRE_THROWS_AS(SyncWait(accept_task()), std::system_error);
}
