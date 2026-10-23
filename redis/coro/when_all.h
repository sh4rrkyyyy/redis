#pragma once

#include <atomic>
#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <tuple>
#include <utility>

namespace redis {
struct Counter {
    size_t cnt{0};
    std::coroutine_handle<> continuation{};
};

template <typename TResult>
class WhenTask {
public:
    struct Promise {
        Promise(Counter& counter, auto&&) : counter(counter) {
        }

        WhenTask<TResult> get_return_object() noexcept {
            return WhenTask{std::coroutine_handle<Promise>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        void unhandled_exception() noexcept {
            std::terminate();
        }

        struct FinalAwaiter {
            bool await_ready() const noexcept {
                return false;
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept {
                auto& counter = h.promise().counter;
                if (--counter.cnt == 0) {
                    return counter.continuation;
                }
                return std::noop_coroutine();
            }

            void await_resume() noexcept {
            }
        };

        FinalAwaiter final_suspend() noexcept {
            return {};
        }

        template <typename U>
        void return_value(U&& value) {
            result = std::forward<U>(value);
        }
        TResult& Result() {
            return *result;
        }

        Counter& counter;
        std::optional<TResult> result;
    };

    using promise_type = Promise;

    WhenTask(const WhenTask&) = delete;
    WhenTask& operator=(const WhenTask&) = delete;

    explicit WhenTask(std::coroutine_handle<Promise> handle) : handle_(handle) {
    }

    WhenTask(WhenTask&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    WhenTask& operator=(WhenTask&& other) noexcept {
        if (this != &other) {
            Clear();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~WhenTask() {
        Clear();
    }

    void Resume() {
        handle_.resume();
    }

    TResult Result() {
        return handle_.promise().Result();
    }

    template <typename Awaitable>
    static WhenTask<TResult> Make(Counter&, Awaitable&& awaitable) {
        co_return co_await std::move(awaitable);
    }

private:
    void Clear() {
        if (handle_) {
            handle_.destroy();
            handle_ = nullptr;
        }
    }

    std::coroutine_handle<Promise> handle_;
};

template <typename... Awaitables>
class WhenAllAwaiter {
public:
    explicit WhenAllAwaiter(Awaitables&&... awaitables)
        : awaitables_(std::forward<Awaitables>(awaitables)...) {
    }

    bool await_ready() const noexcept {
        return false;
    }

    bool await_suspend(std::coroutine_handle<> awaiter) noexcept {
        counter_.continuation = awaiter;
        counter_.cnt = std::tuple_size_v<decltype(awaitables_)>;

        tasks_.emplace(std::apply(
            [this](auto&... awaitables) {
                return std::make_tuple(WhenTask<Result<std::decay_t<decltype(awaitables)>>>::Make(
                    counter_, awaitables)...);
            },
            awaitables_));

        std::apply([](auto&... tasks) { (tasks.Resume(), ...); }, *tasks_);
        return true;
    }

    decltype(auto) await_resume() noexcept {
        return std::apply([](auto&... tasks) { return std::tuple{std::move(tasks).Result()...}; },
                          *tasks_);
    }

    WhenAllAwaiter& operator co_await() & {
        return *this;
    }

    WhenAllAwaiter&& operator co_await() && {
        return std::move(*this);
    }

private:
    template <typename T>
    using Result = std::decay_t<decltype(std::declval<T>().operator co_await().await_resume())>;

    std::tuple<Awaitables...> awaitables_;
    std::optional<std::tuple<WhenTask<Result<Awaitables>>...>> tasks_;
    Counter counter_;
};

template <typename... Awaitables>
auto WhenAll(Awaitables&&... awaitables) {
    return WhenAllAwaiter<std::decay_t<Awaitables>...>{std::forward<Awaitables>(awaitables)...};
}

}  // namespace redis
