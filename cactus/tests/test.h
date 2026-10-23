#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cactus/core/scheduler.h>

namespace cactus {

#define INTERNAL_FIBER_TEST_CASE(TestName, ...)            \
    static void TestName();                                \
    TEST_CASE(__VA_ARGS__) {                               \
        ::cactus::Scheduler scheduler;                     \
        scheduler.Run([&] { CHECK_NOTHROW(TestName()); }); \
    }                                                      \
    static void TestName()

#define INTERNAL_FIBER_MAKE_NAME_IMPL(counter) CactusTest##counter
#define INTERNAL_FIBER_MAKE_NAME(counter) INTERNAL_FIBER_MAKE_NAME_IMPL(counter)

#define FIBER_TEST_CASE(...) \
    INTERNAL_FIBER_TEST_CASE(INTERNAL_FIBER_MAKE_NAME(__COUNTER__), __VA_ARGS__)

}  // namespace cactus
