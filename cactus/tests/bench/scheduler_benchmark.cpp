#include <cactus/cactus.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <thread>

using namespace cactus;

TEST_CASE("SingleFiberYield", "[benchmark]") {
    Scheduler scheduler;
    
    scheduler.Run([&] {
        BENCHMARK("Single fiber yield") {
            Yield();
        };
    });
}

TEST_CASE("SleepingFibers", "[benchmark]") {
    Scheduler scheduler;
    
    scheduler.Run([&] {
        for (int num_fibers : {1, 2, 4, 8, 16, 32, 64, 128, 256}) {
            BENCHMARK_ADVANCED("Sleeping fibers " + std::to_string(num_fibers))
            (Catch::Benchmark::Chronometer meter) {
                std::vector<Fiber> fibers;

                for (int i = 0; i < num_fibers; ++i) {
                    fibers.emplace_back([&] {
                        while (true) {
                            TimeoutGuard timeout(std::chrono::seconds(1));
                            Yield();
                        }
                    });
                }

                meter.measure([&] {
                    Yield();
                });
            };
        }
    });
}

TEST_CASE("SleepingThreads", "[benchmark]") {
    for (int num_threads : {1, 2, 4, 8, 16, 32}) {
        BENCHMARK_ADVANCED("Sleeping threads " + std::to_string(num_threads))
        (Catch::Benchmark::Chronometer meter) {
            std::vector<std::thread> threads;

            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([] {
                    while (true) {
                        std::this_thread::yield();
                    }
                });
            }

            meter.measure([&] {
                std::this_thread::yield();
            });

            for (auto& t : threads) {
                t.detach();
            }
        };
    }
}
