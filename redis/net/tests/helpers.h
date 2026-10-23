#pragma once

#include <coro/common.h>

#include <atomic>
#include <coroutine>
#include <unistd.h>
#include <sys/socket.h>

#include <catch2/catch_test_macros.hpp>

namespace redis::tests {
class TestHelper {
public:
    static int CreateTestSocket() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(sock >= 0);
        int flags = fcntl(sock, F_GETFL, 0);
        REQUIRE(flags >= 0);
        REQUIRE(fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0);
        return sock;
    }

    static int CreatePipeFD() {
        int pipe_fds[2];
#ifdef __linux__
        auto ret = pipe2(pipe_fds, O_NONBLOCK);
        REQUIRE(ret == 0);
#elif defined(__APPLE__)
        auto ret = pipe(pipe_fds);
        REQUIRE(ret == 0);
        int flags = fcntl(pipe_fds[0], F_GETFL, 0);
        REQUIRE(flags >= 0);
        REQUIRE(fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

        flags = fcntl(pipe_fds[1], F_GETFL, 0);
        REQUIRE(flags >= 0);
        REQUIRE(fcntl(pipe_fds[1], F_SETFL, flags | O_NONBLOCK) == 0);
#else
#error "not supported"
#endif
        close(pipe_fds[1]);
        return pipe_fds[0];
    }

    static std::pair<int, int> CreatePipePair() {
        int pipe_fds[2];
#ifdef __linux__
        auto ret = pipe2(pipe_fds, O_NONBLOCK);
        REQUIRE(ret == 0);
#elif defined(__APPLE__)
        auto ret = pipe(pipe_fds);
        REQUIRE(ret == 0);
        int flags = fcntl(pipe_fds[0], F_GETFL, 0);
        REQUIRE(flags >= 0);
        REQUIRE(fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

        flags = fcntl(pipe_fds[1], F_GETFL, 0);
        REQUIRE(flags >= 0);
        REQUIRE(fcntl(pipe_fds[1], F_SETFL, flags | O_NONBLOCK) == 0);
#else
#error "not supported"
#endif
        return {pipe_fds[0], pipe_fds[1]};
    }

    static void ParseEvent(const std::byte* data, uint32_t& flags, int& fd) {
        std::memcpy(&flags, data, sizeof(flags));
        std::memcpy(&fd, data + sizeof(flags), sizeof(fd));
    }
};

struct EventAwaiter {
    std::atomic_flag* flag;

    bool await_ready() const noexcept {
        return false;
    }
    void await_suspend(std::coroutine_handle<>) noexcept {
    }
    void await_resume() noexcept {
        if (flag) {
            flag->test_and_set(std::memory_order_release);
        }
    }
};

struct TestTask {
    struct Promise {
        TestTask get_return_object() {
            return TestTask{std::coroutine_handle<Promise>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        void unhandled_exception() {
            std::terminate();
        }
        void return_void() {
        }
    };

    using promise_type = Promise;

    std::coroutine_handle<Promise> handle;
    ~TestTask() {
        if (handle) {
            handle.destroy();
        }
    }
};

}  // namespace redis::tests
