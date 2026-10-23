#include "task.h"
#include "fmap.h"
#include "when_all.h"
#include "common.h"

#include <catch2/catch_test_macros.hpp>

using namespace redis;  // NOLINT

TEST_CASE("Simple") {
    bool started = false;
    auto coro = [&] -> Task<Unit> {
        started = true;
        co_return Unit{};
    };

    REQUIRE(!started);

    SyncWait([&] -> Task<Unit> {
        auto t = coro();
        REQUIRE(!started);
        co_await t;
        REQUIRE(started);
        co_return Unit{};
    }());

    REQUIRE(started);
}

TEST_CASE("Broken Promise") {
    SyncWait([] -> Task<Unit> {
        Task<Unit> t;
        CHECK_THROWS_AS(co_await t, std::runtime_error);
        co_return Unit{};
    }());
}

TEST_CASE("Async") {
    bool pre_event = false;
    bool post_event = false;
    bool second_wake = false;
    AsyncEvent event;

    auto coro = [&]() -> Task<Unit> {
        pre_event = true;
        co_await event;
        post_event = true;
        co_return Unit{};
    };

    REQUIRE(!pre_event);
    REQUIRE(!post_event);

    SyncWait([&] -> Task<Unit> {
        auto t = coro();
        REQUIRE(!pre_event);
        REQUIRE(!post_event);

        co_await WhenAll(
            [&]() -> Task<Unit> {
                co_await t;
                REQUIRE(pre_event);
                REQUIRE(post_event);
                second_wake = true;
                co_return Unit{};
            }(),
            [&]() -> Task<Unit> {
                REQUIRE(pre_event);
                REQUIRE(!post_event);
                event.Set();
                REQUIRE(post_event);
                co_return Unit{};
            }());

        co_return Unit{};
    }());

    REQUIRE(pre_event);
    REQUIRE(post_event);
    REQUIRE(second_wake);
}

TEST_CASE("When All") {
    int void_task_calls = 0;
    auto make_void_task = [&]() -> Task<Unit> {
        ++void_task_calls;
        co_return Unit{};
    };

    auto make_int_task = [](int x) -> Task<int> { co_return x; };

    auto [v1, i, v2] = SyncWait(WhenAll(make_void_task(), make_int_task(42), make_void_task()));

    (void)v1;
    (void)v2;

    REQUIRE(void_task_calls == 2);
    REQUIRE(i == 42);
}

int Tracker::ctor_count = 0;
int Tracker::copy_count = 0;
int Tracker::move_count = 0;
int Tracker::dtor_count = 0;

TEST_CASE("Args Destruction") {
    Tracker::Reset();

    auto coro = [](Tracker c) -> Task<Tracker> { co_return c; };

    REQUIRE(Tracker::ActiveCount() == 0);

    {
        auto task = coro(Tracker{});
        REQUIRE(Tracker::ActiveCount() == 1);
    }

    REQUIRE(Tracker::ActiveCount() == 0);
}

TEST_CASE("Parameter") {
    Tracker::Reset();

    auto coro = [](Tracker) -> Task<Unit> { co_return Unit{}; };

    Tracker c;

    REQUIRE(Tracker::ActiveCount() == 1);
    REQUIRE(Tracker::ctor_count == 1);
    REQUIRE(Tracker::copy_count == 0);
    REQUIRE(Tracker::move_count == 0);
    REQUIRE(Tracker::dtor_count == 0);

    {
        auto t = coro(c);
        REQUIRE(Tracker::ctor_count == 1);
        REQUIRE(Tracker::ActiveCount() == 2);
    }

    REQUIRE(Tracker::ActiveCount() == 1);
}

TEST_CASE("Fmap Unit") {
    AsyncEvent event;

    auto coro = [&]() -> Task<Unit> {
        co_await event;
        co_return Unit{};
    };

    auto t = coro() | Fmap([](Unit) { return 42; });

    SyncWait(WhenAll(
        [&]() -> Task<Unit> {
            auto answer = co_await t;
            REQUIRE(answer == 42);
            co_return Unit{};
        }(),
        [&]() -> Task<Unit> {
            event.Set();
            co_return Unit{};
        }()));
}

TEST_CASE("Fmap Int") {
    auto answer = []() -> Task<int> { co_return 42; };

    auto t = answer() | Fmap([noise = 1942](auto i) { return i + noise; });
    REQUIRE(SyncWait(t) == 1984);
}

TEST_CASE("Fmap Chain") {
    using namespace std::string_literals;

    auto add_prefix = [](std::string s) {
        return Fmap([s = std::move(s)](const std::string& value) { return s + value; });
    };

    auto add_suffix = [](std::string s) {
        return Fmap([s = std::move(s)](const std::string& value) { return value + s; });
    };

    auto base = [](std::string s) -> Task<std::string> { co_return std::move(s); };

    auto t = base("async"s) | add_prefix("hello "s) | add_suffix(" world"s);

    REQUIRE(SyncWait(t) == "hello async world");
}
