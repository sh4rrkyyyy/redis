#pragma once

#include <coroutine>
#include <atomic>
#include <cstdint>
#include <cassert>
#include <utility>

namespace redis {
class AsyncEvent {
    enum class State {
        NOT_SET,
        NOT_SET_AWAITING,
        SET,
    };

public:
    bool IsSet() const noexcept {
        return state_.load(std::memory_order_acquire) == State::SET;
    }

    void Set() {
        const auto old_state = state_.exchange(State::SET, std::memory_order_acq_rel);
        if (old_state == State::NOT_SET_AWAITING) {
            awaiter_.resume();
        }
    }

    void Reset() noexcept {
        auto old_state = State::SET;
        state_.compare_exchange_strong(old_state, State::NOT_SET, std::memory_order_relaxed);
    }

    auto operator co_await() noexcept {
        struct Awaiter {
            AsyncEvent& event;

            void await_resume() noexcept {
            }

            bool await_ready() const noexcept {
                return event.IsSet();
            }

            bool await_suspend(std::coroutine_handle<> awaiter) {
                event.awaiter_ = awaiter;

                auto old_state = State::NOT_SET;
                return event.state_.compare_exchange_strong(old_state, State::NOT_SET_AWAITING,
                                                            std::memory_order_release,
                                                            std::memory_order_acquire);
            }
        };

        return Awaiter{*this};
    }

private:
    std::atomic<State> state_{};
    std::coroutine_handle<> awaiter_{nullptr};
};

class SyncEvent {
public:
    void Set() noexcept {
        flag_.test_and_set(std::memory_order_release);
        flag_.notify_all();
    }

    void Reset() noexcept {
        flag_.clear(std::memory_order_relaxed);
    }

    void Wait() noexcept {
        flag_.wait(0, std::memory_order_acquire);
    }

private:
    std::atomic_flag flag_;
};

template <typename TResult>
class SyncWaitResult {
public:
    SyncWaitResult(const SyncWaitResult&) = delete;
    SyncWaitResult& operator=(const SyncWaitResult&) = delete;

    struct Promise {
        auto get_return_object() noexcept {
            return SyncWaitResult{std::coroutine_handle<Promise>::from_promise(*this)};
        }

        auto initial_suspend() noexcept {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept {
            struct OnComplete : std::suspend_always {
                void await_suspend(std::coroutine_handle<Promise> handle) const noexcept {
                    handle.promise().event_->Set();
                }
            };
            return OnComplete{};
        }

        void return_value(TResult&& value) noexcept {
            result_ = std::addressof(value);
        }

        void unhandled_exception() {
            throw;
        }

        void Start(SyncEvent& event) {
            event_ = &event;
            std::coroutine_handle<Promise>::from_promise(*this).resume();
        }

        TResult&& Result() {
            return static_cast<TResult&&>(*result_);
        }

        std::remove_reference_t<TResult>* result_;
        SyncEvent* event_{nullptr};
    };

    using promise_type = Promise;

    explicit SyncWaitResult(std::coroutine_handle<Promise> handle) : handle_(handle) {
    }

    ~SyncWaitResult() {
        if (handle_) {
            handle_.destroy();
        }
    }

    void Start(SyncEvent& event) {
        handle_.promise().Start(event);
    }

    decltype(auto) Result() {
        return handle_.promise().Result();
    }

private:
    std::coroutine_handle<Promise> handle_;
};

template <typename Awaitable,
          typename Result = decltype(std::declval<Awaitable>().operator co_await().await_resume())>
SyncWaitResult<Result> MakeSyncWait(Awaitable&& awaitable) {
    co_return co_await std::forward<Awaitable>(awaitable);
}

template <typename Awaitable>
auto SyncWait(Awaitable&& awaitable) {
    auto task = MakeSyncWait(std::forward<Awaitable>(awaitable));

    SyncEvent event;
    task.Start(event);
    event.Wait();
    return task.Result();
}

struct Tracker {
    static int ctor_count;
    static int copy_count;
    static int move_count;
    static int dtor_count;

    static void Reset() {
        ctor_count = copy_count = move_count = dtor_count = 0;
    }

    static int ActiveCount() {
        return ctor_count + copy_count + move_count - dtor_count;
    }

    Tracker() {
        ++ctor_count;
    }

    Tracker(Tracker&&) {
        ++move_count;
    }

    Tracker& operator=(Tracker&&) {
        ++move_count;
        return *this;
    }

    Tracker(const Tracker&) {
        ++copy_count;
    }

    Tracker& operator=(const Tracker&) {
        ++copy_count;
        return *this;
    }

    ~Tracker() {
        ++dtor_count;
    }
};
}  // namespace redis
