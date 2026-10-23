#include "simple_storage.h"
#include "server.h"
#include "utils.h"

#include <utility>
#include <vector>
#include <string>
#include <string_view>
#include <chrono>
#include <thread>

#include "coro/task.h"

#include <cactus/tests/test.h>

#include <catch2/generators/catch_generators.hpp>

using namespace std::chrono_literals;  // NOLINT
using namespace redis;                 // NOLINT

namespace {

using Script = std::vector<std::pair<std::string_view, std::string_view>>;

class MockClient {
public:
    MockClient(Script script, const cactus::SocketAddress& server_addr)
        : script(std::move(script)), server_addr(server_addr), loop(std::make_unique<EventLoop>()) {
    }

    Task<Unit> RunClient() {
        Socket socket;
        co_await socket.Connect(server_addr);

        std::string buffer;

        for (auto [req, resp] : script) {

            co_await socket.Write(cactus::View(req));

            buffer.resize(resp.size());

            co_await socket.Read(cactus::View(buffer));

            REQUIRE(buffer == resp);
        }

        socket.Close();

        co_return Unit{};
    }

    void RunWithServer(Server* server) {
        loop->SetActive();

        bool server_done = false;
        bool client_done = false;
        std::exception_ptr server_error;
        std::exception_ptr client_error;

        auto server_task = [server, &server_done, &server_error]() -> Task<Unit> {
            try {

                co_await server->Run();

                server_done = true;
            } catch (const std::exception& e) {
                server_error = std::current_exception();
                server_done = true;
            }
            co_return Unit{};
        };

        auto client_task = [this, &client_done, &client_error]() -> Task<Unit> {
            try {

                co_await RunClient();
                client_done = true;

            } catch (const std::exception& e) {
                client_error = std::current_exception();
                client_done = true;
            }

            co_return Unit{};
        };

        auto server_handle = server_task();
        auto client_handle = client_task();

        server_handle.Resume();
        client_handle.Resume();

        while (!client_done) {
            loop->RunOnce(100);
        }

        loop->RunOnce(100);
        loop->Stop();

        if (server_error) {
            std::rethrow_exception(server_error);
        }
        if (client_error) {
            std::rethrow_exception(client_error);
        }
    }

    Script script;
    cactus::SocketAddress server_addr;
    std::unique_ptr<EventLoop> loop;
};

}  // namespace

TEST_CASE("StorageGetSet") {
    redis::SimpleStorage storage;

    REQUIRE_FALSE(storage.Get("a").has_value());

    storage.Set("a", "foo");
    REQUIRE(storage.Get("a") == "foo");

    REQUIRE_FALSE(storage.Get("b").has_value());
    storage.Set("b", "baz");
    REQUIRE(storage.Get("b") == "baz");

    storage.Set("a", "bar");
    CHECK(storage.Get("a") == "bar");
    CHECK(storage.Get("b") == "baz");
}

TEST_CASE("StorageDelta") {
    redis::SimpleStorage storage;

    storage.Set("a", "15");
    REQUIRE(storage.Change("a", 5) == 20);
    REQUIRE(storage.Change("a", -10) == 10);
    CHECK(storage.Get("a") == "10");

    storage.Set("b", "-5");
    REQUIRE(storage.Change("b", -12) == -17);
    REQUIRE(storage.Change("b", 15) == -2);
    REQUIRE(storage.Change("b", 0) == -2);
    CHECK(storage.Get("b") == "-2");

    CHECK(storage.Change("c", 1) == 1);
    CHECK(storage.Change("d", -1) == -1);
}

TEST_CASE("StorageNaN") {
    redis::SimpleStorage storage;

    storage.Set("a", "aba");
    CHECK_THROWS(storage.Change("a", 4));

    storage.Set("b", "-1caba");
    CHECK_THROWS(storage.Change("b", 5));

    storage.Set("c", std::string_view{"1\0", 2});
    CHECK_THROWS(storage.Change("c", 6));
}

TEST_CASE("StorageOverflow") {
    constexpr auto kMin = std::numeric_limits<int64_t>::min();
    constexpr auto kMax = std::numeric_limits<int64_t>::max();

    redis::SimpleStorage storage;

    SECTION("Min") {
        storage.Set("x", std::to_string(kMin));
        REQUIRE_THROWS(storage.Change("x", -1));
        REQUIRE_THROWS(storage.Change("x", -4));
        REQUIRE_THROWS(storage.Change("x", kMin));
        REQUIRE(storage.Change("x", kMax) == -1);

        storage.Set("x", "0");
        REQUIRE(storage.Change("x", kMin) == kMin);
        storage.Set("x", "-6");
        REQUIRE(storage.Change("x", kMin + 6) == kMin);
        storage.Set("x", "-7");
        REQUIRE_THROWS(storage.Change("x", kMin + 6));
    }

    SECTION("Max") {
        storage.Set("x", std::to_string(kMax));
        REQUIRE_THROWS(storage.Change("x", 1));
        REQUIRE_THROWS(storage.Change("x", 3));
        REQUIRE_THROWS(storage.Change("x", kMax));
        REQUIRE(storage.Change("x", kMin) == -1);

        storage.Set("x", "0");
        REQUIRE(storage.Change("x", kMax) == kMax);
        storage.Set("x", "12");
        REQUIRE(storage.Change("x", kMax - 12) == kMax);
        storage.Set("x", "13");
        REQUIRE_THROWS(storage.Change("x", kMax - 12));
    }

    SECTION("Positive") {
        storage.Set("x", std::string(100, '1'));
        REQUIRE_THROWS(storage.Change("x", 1));
        REQUIRE_THROWS(storage.Change("x", -1));
    }

    SECTION("Negative") {
        storage.Set("x", '-' + std::string(200, '2'));
        REQUIRE_THROWS(storage.Change("x", 1));
        REQUIRE_THROWS(storage.Change("x", -1));
    }
}

TEST_CASE("ServerStop") {
    auto loop = std::make_unique<EventLoop>();
    loop->SetActive();

    auto server = std::make_shared<Server>(cactus::SocketAddress{"127.0.0.1", 0},
                                           std::make_unique<redis::SimpleStorage>());

    bool server_completed = false;
    std::exception_ptr server_error;

    auto server_task = [server, &server_completed, &server_error]() -> Task<Unit> {
        try {
            co_await server->Run();
            server_completed = true;
        } catch (const std::exception& e) {
            server_error = std::current_exception();
            server_completed = true;
        }
        co_return Unit{};
    };

    auto server_handle = server_task();
    server_handle.Resume();

    SECTION("Stop without any connections") {
        auto stop_task = server->Stop();
        stop_task.Resume();
        server_handle.Resume();

        REQUIRE(server_completed);
    }

    SECTION("Stop after request") {
        Socket client_socket;
        auto connect_task = client_socket.Connect(server->GetAddress());
        connect_task.Resume();
        loop->RunOnce(100);
        REQUIRE(connect_task.IsReady());

        std::string set_cmd = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
        auto write_task = client_socket.Write(cactus::View(set_cmd));
        write_task.Resume();
        loop->RunOnce(100);
        REQUIRE(write_task.IsReady());

        std::string response(8, '\0');
        auto read_task = client_socket.Read(cactus::View(response));
        read_task.Resume();
        for (int i = 0; i < 5; ++i) {
            loop->RunOnce(100);
            if (read_task.IsReady()) {
                break;
            }
        }
        REQUIRE(read_task.IsReady());
        REQUIRE(response == "$2\r\nOK\r\n");
        client_socket.Close();

        auto stop_task = server->Stop();
        stop_task.Resume();
        server_handle.Resume();

        loop->RunOnce(100);

        REQUIRE(server_completed);
    }

    SECTION("Multiple stop calls") {
        auto stop_task1 = server->Stop();
        stop_task1.Resume();
        auto stop_task2 = server->Stop();
        stop_task2.Resume();
        server_handle.Resume();

        REQUIRE(stop_task1.IsReady());
        REQUIRE(stop_task2.IsReady());
        REQUIRE(server_handle.IsReady());
    }
}

TEST_CASE("GetSet") {
    Script script = {{"*3\r\n"
                      "$3\r\nSET\r\n"
                      "$3\r\nkey\r\n"
                      "$5\r\nvalue\r\n",
                      "$2\r\nOK\r\n"},
                     {"*2\r\n"
                      "$3\r\nGET\r\n"
                      "$3\r\nkey\r\n",
                      "$5\r\nvalue\r\n"},
                     {"*2\r\n"
                      "$3\r\nGET\r\n"
                      "$7\r\nnot-key\r\n",
                      "$-1\r\n"},
                     {"*3\r\n"
                      "$3\r\nSET\r\n"
                      "$3\r\nkey\r\n"
                      "$7\r\nanother\r\n",
                      "$2\r\nOK\r\n"},
                     {"*2\r\n"
                      "$3\r\nGET\r\n"
                      "$3\r\nkey\r\n",
                      "$7\r\nanother\r\n"}};

    auto server = std::make_shared<Server>(cactus::SocketAddress{"127.0.0.1", 0},
                                           std::make_unique<redis::SimpleStorage>());
    auto client = std::make_shared<MockClient>(std::move(script), server->GetAddress());

    client->RunWithServer(server.get());
}

TEST_CASE("IncrementDecrement") {
    Script script = {{"*2\r\n"
                      "$4\r\nINCR\r\n"
                      "$3\r\nkey\r\n",
                      ":1\r\n"},
                     {"*2\r\n"
                      "$4\r\nINCR\r\n"
                      "$3\r\nkey\r\n",
                      ":2\r\n"},
                     {"*2\r\n"
                      "$4\r\nDECR\r\n"
                      "$3\r\nkey\r\n",
                      ":1\r\n"},
                     {"*2\r\n"
                      "$4\r\nDECR\r\n"
                      "$3\r\nkey\r\n",
                      ":0\r\n"},
                     {"*2\r\n"
                      "$4\r\nDECR\r\n"
                      "$3\r\nkey\r\n",
                      ":-1\r\n"}};

    auto server = std::make_shared<Server>(cactus::SocketAddress{"127.0.0.1", 0},
                                           std::make_unique<redis::SimpleStorage>());
    auto client = std::make_shared<MockClient>(std::move(script), server->GetAddress());

    client->RunWithServer(server.get());
}

TEST_CASE("IncrementAfterSet") {
    Script script = {{"*3\r\n"
                      "$3\r\nSET\r\n"
                      "$4\r\ndoor\r\n"
                      "$2\r\n-3\r\n",
                      "$2\r\nOK\r\n"},
                     {"*2\r\n"
                      "$4\r\nINCR\r\n"
                      "$4\r\ndoor\r\n",
                      ":-2\r\n"},
                     {"*2\r\n"
                      "$4\r\nINCR\r\n"
                      "$4\r\ndoor\r\n",
                      ":-1\r\n"},
                     {"*2\r\n"
                      "$3\r\nGET\r\n"
                      "$4\r\ndoor\r\n",
                      "$2\r\n-1\r\n"}};

    auto server = std::make_shared<Server>(cactus::SocketAddress{"127.0.0.1", 0},
                                           std::make_unique<redis::SimpleStorage>());
    auto client = std::make_shared<MockClient>(std::move(script), server->GetAddress());

    client->RunWithServer(server.get());
}
