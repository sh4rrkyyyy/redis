#pragma once

#include <coroutine>
#include <variant>
#include <stdexcept>
#include <exception>
#include <utility>
#include <cassert>
#include <optional>

namespace redis {

// void
struct Unit {};

template <typename Promise>
class Awaiter {
public:
    explicit Awaiter(std::coroutine_handle<Promise> h) : handle_(h) {
    }

    bool await_ready() const noexcept {
        return !handle_ || handle_.done();
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
        handle_.promise().continuation_ = awaiting;
        return handle_;
    }

    decltype(auto) await_resume() {
        if (!handle_) {
            throw std::runtime_error{"Broken promise"};
        }
        return std::move(handle_.promise().Result());
    }

private:
    std::coroutine_handle<Promise> handle_{};
};

template <typename T>
class Task;

template <typename T>
class Promise {
public:
    Task<T> get_return_object() noexcept;

    void unhandled_exception() noexcept {
        exception_ = std::current_exception();
    }

    template <typename U>
    void return_value(U&& value) {
        res_ = std::forward<U>(value);
    }

    std::suspend_always initial_suspend() noexcept {
        return {};
    }

    struct FinalAwaiter {
        bool await_ready() const noexcept {
            return false;
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept {
            auto continuation = h.promise().continuation_;
            if (continuation) {
                return continuation;
            }
            return std::noop_coroutine();
        }
        void await_resume() noexcept {
        }
    };

    FinalAwaiter final_suspend() noexcept {
        return {};
    }

    T& Result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return *res_;
    }

private:
    template <typename>
    friend class Awaiter;
    std::coroutine_handle<> continuation_{};
    std::optional<T> res_;
    std::exception_ptr exception_;
};

template <typename T>
class Task {
public:
    using promise_type = Promise<T>;
    using Handle = std::coroutine_handle<promise_type>;

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task() = default;
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            Clear();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    explicit Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {
    }

    ~Task() {
        Clear();
    }

    bool IsReady() const {
        return handle_.done();
    }
    void Resume() {
        return handle_.resume();
    }

    Awaiter<promise_type> operator co_await() const {
        return Awaiter{handle_};
    }

private:
    void Clear() {
        if (handle_) {
            handle_.destroy();
            handle_ = nullptr;
        }
    }
    Handle handle_{};
};

template <typename T>
Task<T> Promise<T>::get_return_object() noexcept {
    return Task{Task<T>::Handle::from_promise(*this)};
}

}  // namespace redis
