#include "test.h"
#include <cactus/cactus.h>

using namespace cactus;

static const std::array<char, 4> kPing = {'p', 'i', 'n', 'g'};
static const std::array<char, 4> kPong = {'p', 'o', 'n', 'g'};

static const auto kLoopback = cactus::SocketAddress("127.0.0.1", 0);

FIBER_TEST_CASE("Simple ping pong") {
    auto listener = ListenTCP(kLoopback);

    Fiber run_listener([&] {
        while (true) {
            auto conn = listener->Accept();

            std::array<char, 4> buf = {};
            conn->ReadFull(View(buf));

            REQUIRE(buf == kPing);

            conn->Write(View(kPong));
        }
    });

    for (int i = 0; i < 4; i++) {
        auto conn = DialTCP(listener->Address());
        conn->Write(View(kPing));

        std::array<char, 4> buf = {};
        conn->ReadFull(View(buf));

        REQUIRE(buf == kPong);
    }
}

FIBER_TEST_CASE("Close blocked socket") {
    auto listener = ListenTCP(kLoopback);

    Fiber run_listener([&] { REQUIRE_THROWS_AS(listener->Accept(), std::runtime_error); });

    Fiber stop([&] { listener->Close(); });

    Yield();
    Yield();
}

FIBER_TEST_CASE("Write to broken socket") {
    auto listener = ListenTCP(kLoopback);

    WaitGroup g;
    g.Spawn([&] { listener->Accept(); });

    auto conn = DialTCP(listener->Address());

    g.Wait();

    auto writeLoop = [&] {
        while (true) {
            conn->Write(View(kPing));
        }
    };

    REQUIRE_THROWS_AS(writeLoop(), std::system_error);

    std::array<char, 4> buf{};
    REQUIRE(conn->Read(View(buf)) == 0);
}

FIBER_TEST_CASE("Error listening closed socket") {
    auto listener = ListenTCP(kLoopback);

    listener->Close();

    REQUIRE_THROWS_AS(listener->Accept(), std::runtime_error);
}

FIBER_TEST_CASE("Cancel listen") {
    auto listener = ListenTCP(kLoopback);

    Fiber run_listener([&] { listener->Accept(); });

    Yield();
}

FIBER_TEST_CASE("Connect timeout") {
    auto listener = ListenTCP(kLoopback);

    TimeoutGuard guard(std::chrono::microseconds(1));
    REQUIRE_THROWS_AS(DialTCP(listener->Address()), TimeoutException);
}

FIBER_TEST_CASE("Cancel dial") {
    auto listener = ListenTCP(kLoopback);

    Fiber run_dialer([&] { DialTCP(listener->Address()); });

    Yield();
}

FIBER_TEST_CASE("Test Address") {
    {
        SocketAddress address{"127.0.0.1", 10'000};
        CHECK(address.GetPort() == 10'000);
        CHECK(address == address);
        address.SetPort(10'001);
        CHECK(address.GetPort() == 10'001);

        auto address2 = address;
        CHECK(address == address2);
        address.SetPort(10'000);
        CHECK(address != address2);
    }
    {
        SocketAddress address{"localhost", 10'000, true};
        CHECK(address.GetPort() == 10'000);
        CHECK(address == address);
        address.SetPort(10'001);
        CHECK(address.GetPort() == 10'001);
    }
}
