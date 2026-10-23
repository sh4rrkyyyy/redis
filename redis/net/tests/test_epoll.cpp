#include "epoll.h"
#include "helpers.h"

#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <atomic>
#include <thread>

using namespace redis;         // NOLINT
using namespace redis::tests;  // NOLINT

TEST_CASE("Ctor/dtor") {
    SECTION("Default") {
        REQUIRE_NOTHROW(Epoll());
    }

    SECTION("Multiple") {
        Epoll epoll1;
        Epoll epoll2;
        REQUIRE_NOTHROW(epoll1);
        REQUIRE_NOTHROW(epoll2);
    }
}

TEST_CASE("Registration") {
    Epoll epoll;
    int fd = TestHelper::CreateTestSocket();

    SECTION("Read") {
        REQUIRE(epoll.Register(fd, Epoll::READ) == 0);
        REQUIRE(epoll.Unregister(fd) == 0);
    }

    SECTION("Multiple events") {
        REQUIRE(epoll.Register(fd, Epoll::READ | Epoll::WRITE) == 0);
        REQUIRE(epoll.Unregister(fd) == 0);
    }

    SECTION("Twice - failed") {
        REQUIRE(epoll.Register(fd, Epoll::READ) == 0);
        REQUIRE(epoll.Register(fd, Epoll::READ) != 0);
        REQUIRE(epoll.Unregister(fd) == 0);
    }

    close(fd);
}

TEST_CASE("Modify") {
    Epoll epoll;
    int fd = TestHelper::CreateTestSocket();

    SECTION("Registered") {
        REQUIRE(epoll.Register(fd, Epoll::READ) == 0);
        REQUIRE(epoll.Modify(fd, Epoll::WRITE) == 0);
        REQUIRE(epoll.Unregister(fd) == 0);
    }

    SECTION("Unregistered") {
        REQUIRE(epoll.Modify(fd, Epoll::READ) != 0);
    }

    SECTION("Multiple events") {
        REQUIRE(epoll.Register(fd, Epoll::READ) == 0);
        REQUIRE(epoll.Modify(fd, Epoll::READ | Epoll::WRITE) == 0);
        REQUIRE(epoll.Unregister(fd) == 0);
    }

    close(fd);
}

TEST_CASE("Poll no events") {
    Epoll epoll;
    std::array<std::byte, 1024> buffer;
    MutableView view(buffer.data(), buffer.size());

    int result = epoll.Poll(10, view);
    REQUIRE(result == 0);

    result = epoll.Poll(0, view);
    REQUIRE(result == 0);
}

TEST_CASE("Poll read") {
    Epoll epoll;
    auto [read_fd, write_fd] = TestHelper::CreatePipePair();

    REQUIRE(epoll.Register(read_fd, Epoll::READ) == 0);

    const char* test_data = "test data";
    ssize_t written = write(write_fd, test_data, strlen(test_data) + 1);
    REQUIRE(written > 0);

    std::array<std::byte, 1024> buffer;
    MutableView view(buffer.data(), buffer.size());

    SECTION("Read") {
        int result = epoll.Poll(1000, view);
        REQUIRE(result == 1);

        uint32_t flags;
        int fd;
        TestHelper::ParseEvent(view.data(), flags, fd);

        CHECK(fd == read_fd);
        CHECK((flags & Epoll::READ) != 0);
    }

    SECTION("Multiple events") {
        written = write(write_fd, test_data, strlen(test_data) + 1);
        REQUIRE(written > 0);

        int result = epoll.Poll(1000, view);
        REQUIRE(result == 1);

        result = epoll.Poll(0, view);
        REQUIRE(result == 1);
    }

    REQUIRE(epoll.Unregister(read_fd) == 0);
    close(read_fd);
    close(write_fd);
}

TEST_CASE("Poll write") {
    Epoll epoll;
    int sock = TestHelper::CreateTestSocket();

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

    REQUIRE(epoll.Register(sock, Epoll::WRITE) == 0);

    std::array<std::byte, 1024> buffer;
    MutableView view(buffer.data(), buffer.size());

    for (int ret = epoll.Poll(1000, view);;) {
        REQUIRE(ret >= 0);
        if (ret) {
            uint32_t flags;
            int fd;
            TestHelper::ParseEvent(view.data(), flags, fd);

            CHECK(fd == sock);
            CHECK((flags & Epoll::WRITE) != 0);
            break;
        }
    }

    REQUIRE(epoll.Unregister(sock) == 0);
    close(sock);
}

TEST_CASE("Multiple events") {
    Epoll epoll;
    std::vector<std::pair<int, int>> pipes;
    constexpr int kNumPipes = 5;

    for (int i = 0; i < kNumPipes; ++i) {
        auto [read_fd, write_fd] = TestHelper::CreatePipePair();
        pipes.emplace_back(read_fd, write_fd);
        REQUIRE(epoll.Register(read_fd, Epoll::READ) == 0);

        char data[] = "test";
        REQUIRE(write(write_fd, data, sizeof(data)) == sizeof(data));
    }

    std::array<std::byte, 1024> buffer;
    MutableView view(buffer.data(), buffer.size());

    int result = epoll.Poll(1000, view);
    REQUIRE(result == kNumPipes);
    std::vector<int> received_fds;
    for (int i = 0; i < result; ++i) {
        uint32_t flags;
        int fd;
        TestHelper::ParseEvent(view.data() + i * 8, flags, fd);
        received_fds.push_back(fd);
        CHECK((flags & Epoll::READ) != 0);
    }

    for (auto& [read_fd, write_fd] : pipes) {
        CHECK(std::find(received_fds.begin(), received_fds.end(), read_fd) != received_fds.end());
    }

    for (auto& [read_fd, write_fd] : pipes) {
        REQUIRE(epoll.Unregister(read_fd) == 0);
        close(read_fd);
        close(write_fd);
    }
}

TEST_CASE("Small buffer") {
    Epoll epoll;
    auto [read_fd, write_fd] = TestHelper::CreatePipePair();

    REQUIRE(epoll.Register(read_fd, Epoll::READ) == 0);

    char data[] = "test";
    REQUIRE(write(write_fd, data, sizeof(data)) == sizeof(data));

    std::array<std::byte, 1> small_buffer;
    MutableView view(small_buffer.data(), small_buffer.size());

    int result = epoll.Poll(100, view);
    CHECK(result == -1);
    CHECK(errno == EINVAL);

    REQUIRE(epoll.Unregister(read_fd) == 0);
    close(read_fd);
    close(write_fd);
}

TEST_CASE("Wakeup") {
    Epoll epoll;
    auto [read_fd, write_fd] = TestHelper::CreatePipePair();

    REQUIRE(epoll.Register(read_fd, Epoll::READ) == 0);

    std::atomic_flag poll_started;

    int poll_result{0};

    std::thread poll_thread([&]() {
        std::array<std::byte, 1024> buffer;
        MutableView view(buffer.data(), buffer.size());
        poll_started.test_and_set();
        poll_started.notify_one();
        poll_result = epoll.Poll(5000, view);
    });

    poll_started.wait(0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    epoll.Close();

    poll_thread.join();

    CHECK(poll_result == -1);

    REQUIRE(epoll.Unregister(read_fd) == 0);
    close(read_fd);
    close(write_fd);
}

TEST_CASE("Infinite wait") {
    Epoll epoll;
    auto [read_fd, write_fd] = TestHelper::CreatePipePair();

    REQUIRE(epoll.Register(read_fd, Epoll::READ) == 0);

    std::atomic_flag poll_started;
    int poll_result{};

    std::thread poll_thread([&]() {
        std::array<std::byte, 1024> buffer;
        MutableView view(buffer.data(), buffer.size());
        poll_started.test_and_set();
        poll_started.notify_one();
        poll_result = epoll.Poll(-1, view);
    });

    poll_started.wait(0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    char data[] = "test";
    REQUIRE(write(write_fd, data, sizeof(data)) == sizeof(data));

    poll_thread.join();
    CHECK(poll_result == 1);

    REQUIRE(epoll.Unregister(read_fd) == 0);
    close(read_fd);
    close(write_fd);
}
