#include <cactus/cactus.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <glog/logging.h>

using namespace cactus;

const auto kLoopback = cactus::SocketAddress("127.0.0.1", 0);

TEST_CASE("ServerBenchmark", "[benchmark]") {
    Scheduler sched;
    sched.Run([&] {
        auto lsn = ListenTCP(kLoopback);
        Fiber server([&] {
            auto conn = lsn->Accept();

            while (true) {
                int n[1] = {0};
                conn->ReadFull(View(n));
                n[0]++;
                conn->Write(View(n));
            }
        });

        auto conn = DialTCP(lsn->Address());
        int n = 0;

        BENCHMARK("Server ping-pong") {
            int i[1] = {n};
            
            conn->Write(View(i));
            conn->ReadFull(View(i));
            
            if (n + 1 != i[0]) {
                LOG(FATAL) << n << " + 2 != " << i[0];
            }
            
            n = i[0];
        };
    });
}
