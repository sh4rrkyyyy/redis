#include <cactus/cactus.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <glog/logging.h>

using namespace cactus;

static const std::array<char, 4> kPing = {'p', 'i', 'n', 'g'};
static const std::array<char, 4> kPong = {'p', 'o', 'n', 'g'};

const auto kLoopback = cactus::SocketAddress("127.0.0.1", 0);
constexpr int kParallelRequests = 10;

namespace {
void SimpleServer(IListener* lsn) {
    while (true) {
        auto client = lsn->Accept();

        DLOG(INFO) << "Accepted client " << client->RemoteAddress();

        std::array<char, 4> buf;
        client->ReadFull(View(buf));

        client->Write(View(kPong));

        DLOG(INFO) << "Finished handling client " << client->RemoteAddress();
    }
}

void ConcurrentServer(IListener* lsn) {
    ServerGroup g;
    while (true) {
        std::shared_ptr<IConn> client{lsn->Accept()};

        g.Spawn([client] {
            DLOG(INFO) << "Accepted client " << client->RemoteAddress();

            std::array<char, 4> buf;
            client->ReadFull(View(buf));

            client->Write(View(kPong));

            DLOG(INFO) << "Finished handling client " << client->RemoteAddress();
        });
    }
}

}  // namespace

TEST_CASE("ServerBenchmark", "[benchmark]") {
    Scheduler sched;
    sched.Run([&] {
        auto lsn = ListenTCP(kLoopback);
        
        SECTION("Simple server") {
            Fiber server([&] {
                SimpleServer(lsn.get());
            });
            
            BENCHMARK_ADVANCED("Simple server with " + std::to_string(kParallelRequests) + " parallel requests")
            (Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                    WaitGroup g;
                    for (int i = 0; i < kParallelRequests; i++) {
                        g.Spawn([&, i] {
                            auto conn = DialTCP(lsn->Address());
                            
                            DLOG(INFO) << i << " sending ping";
                            conn->Write(View(kPing));
                            
                            std::array<char, 4> buf;
                            conn->ReadFull(View(buf));
                            DLOG(INFO) << i << " received pong";
                        });
                    }
                    g.Wait();
                });
            };
        }
        
        SECTION("Concurrent server") {
            Fiber server([&] {
                ConcurrentServer(lsn.get());
            });
            
            BENCHMARK_ADVANCED("Concurrent server with " + std::to_string(kParallelRequests) + " parallel requests")
            (Catch::Benchmark::Chronometer meter) {
                meter.measure([&] {
                    WaitGroup g;
                    for (int i = 0; i < kParallelRequests; i++) {
                        g.Spawn([&, i] {
                            auto conn = DialTCP(lsn->Address());
                            
                            DLOG(INFO) << i << " sending ping";
                            conn->Write(View(kPing));
                            
                            std::array<char, 4> buf;
                            conn->ReadFull(View(buf));
                            DLOG(INFO) << i << " received pong";
                        });
                    }
                    g.Wait();
                });
            };
        }
    });
}
