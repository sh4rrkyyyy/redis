#include "test.h"
#include <cactus/cactus.h>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>

// NOLINTNEXTLINE
using namespace cactus;

namespace {
struct TestListTag {};

template <class Fn>
struct FnContextEntry : public IContextEntry {
    FnContextEntry(Fn fn) : fn(std::move(fn)) {
    }

    virtual SavedContext* Run() override {
        return fn();
    }

    Fn fn;
};

} // namespace

TEST_CASE("Link push back") {
    List<TestListTag> list;

    ListElement<TestListTag> a;
    ListElement<TestListTag> b;

    list.PushBack(&a);
    list.PushBack(&b);

    REQUIRE(list.Begin() == &a);
    REQUIRE(list.Back() == &b);
}

TEST_CASE("Simple context switch") {
    std::vector<char> stack(512 * 1024);
    SavedContext fiber_context{stack.data(), stack.size()};
    SavedContext main_context;

    bool activated = false;
    bool activated_twice = false;
    FnContextEntry switch_back = [&]() -> SavedContext* {
        activated = true;
        main_context.Activate(&fiber_context);
        activated_twice = true;
        return &main_context;
    };
    fiber_context.entry = &switch_back;

    fiber_context.Activate(&main_context);
    REQUIRE(activated == true);
    REQUIRE(activated_twice != true);

    fiber_context.Activate(&main_context);
    REQUIRE(activated == true);
    REQUIRE(activated_twice == true);
}

TEST_CASE("Exception context saved and restored on switch") {
    std::vector<char> stack(512 * 1024);
    SavedContext fiber_context{stack.data(), stack.size()};
    SavedContext main_context;

    FnContextEntry throw_exceptions = [&]() -> SavedContext* {
        try {
            throw std::exception();
        } catch (std::exception& ex) {
            auto my_exception = std::current_exception();
            REQUIRE(std::uncaught_exceptions() == 0);

            main_context.Activate(&fiber_context);

            REQUIRE(my_exception == std::current_exception());
            REQUIRE(std::uncaught_exceptions() == 0);
        }

        return &main_context;
    };
    fiber_context.entry = &throw_exceptions;

    fiber_context.Activate(&main_context);
    try {
        throw std::exception();
    } catch (std::exception& ex) {
        auto my_exception = std::current_exception();
        REQUIRE(std::uncaught_exceptions() == 0);

        fiber_context.Activate(&main_context);

        REQUIRE(my_exception == std::current_exception());
        REQUIRE(std::uncaught_exceptions() == 0);
    }
}

#if defined(__x86_64__)

__attribute__((naked)) static uint64_t ReadSp() {
    asm(R"(
        mov %rsp, %rax
        add $8, %rax
        ret
    )");
}

#elif defined(__aarch64__)

__attribute__((naked)) static uint64_t ReadSp() {
    asm(R"(
        mov x0, sp
        ret
    )");
}

#else
#error Unsupported architecture
#endif

TEST_CASE("Stack alignment") {
    cactus::Scheduler sched;
    sched.Run([] { REQUIRE(ReadSp() % 16 == 0); });
}

TEST_CASE("FP context is preserved") {
    cactus::Scheduler sched;
    sched.Run([] {
        cactus::WaitGroup wg;
        wg.Spawn([] {
            volatile double x = 1.5;
            double y = 0.5;
            for (int i = 0; i < 5; ++i) {
                x += 1;
                REQUIRE(x == double(i + 2.5));
                REQUIRE(y == double(i + 0.5));
                y += 1;
                cactus::Yield();
            }
        });

        wg.Spawn([] {
            volatile double x = 15.0;
            double y = 0.0;
            for (int i = 0; i < 5; ++i) {
                REQUIRE(x == double(i + 15));
                x += 1;
                y += 1;
                REQUIRE(y == double(i + 1));
                cactus::Yield();
            }
        });

        wg.Wait();
    });
}

FIBER_TEST_CASE("Wait queue cancelation") {
    WaitQueue wq;

    ServerGroup g;
    bool woken_up = false;

    {
        ServerGroup canceled_g;
        canceled_g.Spawn([&] {
            auto finally = cactus::MakeGuard([&] { wq.NotifyOne(); });

            wq.Wait();
        });

        canceled_g.Spawn([&] { wq.Wait(); });

        g.Spawn([&] {
            wq.Wait();
            woken_up = true;
        });

        SleepFor(std::chrono::milliseconds(1));
    }

    SleepFor(std::chrono::milliseconds(1));

    REQUIRE(woken_up);
}
