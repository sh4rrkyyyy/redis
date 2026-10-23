#pragma once

#include <atomic>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <vector>

struct Group {
    std::coroutine_handle<> continuation{std::noop_coroutine()};
    size_t counter{0};
};

class CoroutineGroupTask {
public:
    struct Promise {
        Promise(Group& group, auto&&) : group(group) {
        }

        CoroutineGroupTask get_return_object() noexcept {
            return CoroutineGroupTask{std::coroutine_handle<Promise>::from_promise(*this)};
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
                auto& g = h.promise().group;
                g.counter--;
                if (g.counter == 0) {
                    return g.continuation;
                }
                return std::noop_coroutine();
            }

            void await_resume() noexcept {
            }
        };

        FinalAwaiter final_suspend() noexcept {
            return {};
        }

        void return_void() {
        }

        Group& group;
    };

    using promise_type = Promise;

    CoroutineGroupTask(const CoroutineGroupTask&) = delete;
    CoroutineGroupTask& operator=(const CoroutineGroupTask&) = delete;

    explicit CoroutineGroupTask(std::coroutine_handle<Promise> handle) : handle_(handle) {
    }

    CoroutineGroupTask(CoroutineGroupTask&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    CoroutineGroupTask& operator=(CoroutineGroupTask&& other) noexcept {
        if (this != &other) {
            Clear();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~CoroutineGroupTask() {
        Clear();
    }

    void Resume() {
        handle_.resume();
    }

    template <typename Awaitable>
    static CoroutineGroupTask Make(Group&, Awaitable awaitable) {
        co_await std::move(awaitable);
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

struct JoinAwaiter {
    bool await_ready() const noexcept {
        return group.counter == 0;
    }
    void await_suspend(std::coroutine_handle<> continuation) noexcept {
        group.continuation = continuation;
    }

    void await_resume() noexcept {
    }

    Group& group;
};

class CoroutineGroup {
public:
    template <typename Awaitable>
    void Spawn(Awaitable&& awaitable) {
        group_.counter++;
        tasks_.push_back(CoroutineGroupTask::Make(group_, std::forward<Awaitable>(awaitable)));
        tasks_.back().Resume();
    }

    [[nodiscard]] auto Join() noexcept {
        return JoinAwaiter{group_};
    }

private:
    Group group_;
    std::vector<CoroutineGroupTask> tasks_;
};
