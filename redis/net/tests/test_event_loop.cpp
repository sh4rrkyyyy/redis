#include "event_loop.h"
#include "helpers.h"

#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <thread>

using namespace redis;         // NOLINT
using namespace redis::tests;  // NOLINT

TEST_CASE("Ctor/Dtor") {
    SECTION("Default") {
        REQUIRE_NOTHROW(EventLoop());
    }

    SECTION("Active is set") {
        {
            EventLoop loop;
            REQUIRE(EventLoop::active == &loop);
        }
        REQUIRE(EventLoop::active == nullptr);
    }
}

TEST_CASE("Register") {
    EventLoop loop;

    SECTION("Valid") {
        int fd = TestHelper::CreateTestSocket();
        std::atomic_flag completed{false};

        auto run = [&]() -> TestTask { co_await EventAwaiter{&completed}; };

        TestTask task = run();
        REQUIRE(loop.Register(fd, Epoll::READ, task.handle) == 0);

        loop.Unregister(fd);
        close(fd);
    }

    SECTION("No throw") {
        int fd = TestHelper::CreateTestSocket();
        REQUIRE_NOTHROW(loop.Unregister(fd));
        close(fd);
    }

    SECTION("Null") {
        int fd = TestHelper::CreateTestSocket();
        REQUIRE(loop.Register(fd, Epoll::READ, std::coroutine_handle<>{}) != 0);
        close(fd);
    }
}

TEST_CASE("Run&Stop") {
    SECTION("Stop") {
        EventLoop loop;
        std::atomic_flag loop_running;
        bool loop_stopped{false};

        std::thread loop_thread([&]() {
            loop.SetActive();
            loop_running.test_and_set();
            loop_running.notify_one();
            loop.Run();
            loop_stopped = true;
        });

        loop_running.wait(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        loop.Stop();
        loop_thread.join();

        REQUIRE(loop_stopped);
    }

    SECTION("Multiple Stops") {
        EventLoop loop;
        loop.Stop();
        loop.Stop();
    }
}

TEST_CASE("Wakeup") {
    EventLoop loop;
    auto [read_fd, write_fd] = TestHelper::CreatePipePair();

    std::atomic_flag coroutine_resumed{false};

    auto run = [&]() -> TestTask { co_await EventAwaiter{&coroutine_resumed}; };

    TestTask task = run();

    REQUIRE(loop.Register(read_fd, Epoll::READ, task.handle) == 0);

    std::atomic_flag loop_started{false};
    std::thread loop_thread([&]() {
        loop.SetActive();
        loop_started.test_and_set(std::memory_order_release);
        loop_started.notify_one();
        loop.Run();
    });

    loop_started.wait(false, std::memory_order_acquire);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    char data[] = "test";
    ssize_t written = write(write_fd, data, sizeof(data));
    REQUIRE(written > 0);

    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(100);

    while (!coroutine_resumed.test(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            FAIL("Coroutine was not resumed within timeout");
            break;
        }
        std::this_thread::yield();
    }

    CHECK(coroutine_resumed.test(std::memory_order_acquire));

    loop.Stop();
    loop_thread.join();

    close(read_fd);
    close(write_fd);
}

TEST_CASE("Socket Connection") {
    EventLoop loop;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(listen_fd >= 0);
    int flags = fcntl(listen_fd, F_GETFL, 0);
    REQUIRE(flags >= 0);
    REQUIRE(fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) == 0);

    int opt = 1;
    REQUIRE(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    REQUIRE(bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    REQUIRE(listen(listen_fd, 5) == 0);

    socklen_t addr_len = sizeof(addr);
    REQUIRE(getsockname(listen_fd, (struct sockaddr*)&addr, &addr_len) == 0);

    std::atomic_flag connection_received;
    std::atomic<int> accepted_fd{-1};

    auto run = [&]() -> TestTask {
        co_await EventAwaiter{&connection_received};
        int fd = accept(listen_fd, nullptr, nullptr);
        accepted_fd.store(fd, std::memory_order_release);
    };

    TestTask task = run();
    int register_result = loop.Register(listen_fd, Epoll::READ, task.handle);
    REQUIRE(register_result == 0);

    std::thread loop_thread([&]() {
        loop.SetActive();
        loop.Run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(client_fd >= 0);

    REQUIRE(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    const auto timeout = std::chrono::milliseconds(1000);
    auto start = std::chrono::steady_clock::now();

    while (!connection_received.test(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            break;
        }
        std::this_thread::yield();
    }

    CHECK(connection_received.test(std::memory_order_acquire));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int accepted = accepted_fd.load(std::memory_order_acquire);
    CHECK(accepted >= 0);

    close(accepted);

    loop.Stop();
    loop_thread.join();

    close(client_fd);
    close(listen_fd);
}
