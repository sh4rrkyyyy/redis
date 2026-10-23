#include <functional>
#include <cactus/cactus.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

using namespace cactus;

namespace {
struct FnContextEntry : public IContextEntry {
    template <class Fn>
    FnContextEntry(Fn fn) : fn(fn) {
    }

    std::function<SavedContext*()> fn;

    virtual SavedContext* Run() override {
        return fn();
    }
};
}  // namespace

TEST_CASE("Benchmark") {
    std::vector<char> stack(4096);
    SavedContext fiber_context(stack.data(), stack.size());
    SavedContext main_context;

    bool exit = false;

    FnContextEntry run = [&] {
        while (!exit) {
            main_context.Activate(&fiber_context);
        }

        return &main_context;
    };
    fiber_context.entry = &run;

    BENCHMARK("ContextSwitch") {
        fiber_context.Activate(&main_context);
    };

    exit = true;
    fiber_context.Activate(&main_context);
}
