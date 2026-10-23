#pragma once

#include <coroutine>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace redis {
template <typename F, typename Awaiter>
struct FMapAwaiter {
    using Result =
        std::decay_t<std::invoke_result_t<F, decltype(std::declval<Awaiter>().await_resume())>>;

    FMapAwaiter(F func, Awaiter awaiter, std::optional<Result>* result)
        : func(std::move(func)), awaiter(std::move(awaiter)), result(result) {
    }

    decltype(auto) await_ready() {
        return awaiter.await_ready();
    }

    template <typename Promise>
    decltype(auto) await_suspend(std::coroutine_handle<Promise> coro) {
        return awaiter.await_suspend(coro);
    }

    decltype(auto) await_resume() {
        result->emplace(func(awaiter.await_resume()));
        return std::move(**result);
    }

    F func;
    Awaiter awaiter;
    std::optional<Result>* result;
};

template <typename F, typename Awaitable>
class FMapAwaitable {
public:
    using Awaiter = decltype(std::declval<Awaitable&>().operator co_await());
    using Result = typename FMapAwaiter<F, Awaiter>::Result;

    FMapAwaitable(F func, Awaitable awaitable)
        : func_(std::move(func)), awaitable_(std::move(awaitable)) {
    }

    auto operator co_await() {
        return FMapAwaiter{std::move(func_), awaitable_.operator co_await(), &result_};
    }

private:
    F func_;
    Awaitable awaitable_;
    std::optional<Result> result_;
};

template <typename F>
struct FMapTransform {
    explicit FMapTransform(F&& f) : func(std::forward<F>(f)) {
    }

    F func;
};

template <typename F>
auto Fmap(F&& func) {
    return FMapTransform<std::decay_t<F>>{std::forward<F>(func)};
}

template <typename F, typename Awaitable>
auto Fmap(F&& func, Awaitable&& awaitable) {
    return FMapAwaitable<std::decay_t<F>, std::decay_t<Awaitable>>{
        std::forward<F>(func), std::forward<Awaitable>(awaitable)};
}

template <typename T, typename F>
decltype(auto) operator|(T&& value, const FMapTransform<F>& transform) {
    return Fmap(transform.func, std::forward<T>(value));
}

}  // namespace redis
