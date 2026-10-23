#include "client.h"
#include "coro/task.h"
#include "coro/common.h"
#include "coro/when_all.h"
#include "io/async_listener.h"

#include <utility>
#include <vector>
#include <algorithm>
#include <string>
#include <memory>
#include <tuple>
#include <optional>
#include <thread>

#include <cactus/cactus.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace redis;  // NOLINT

namespace {

using Script = std::vector<std::pair<std::string_view, std::string_view>>;

class MockServer {
public:
    MockServer(Script script)
        : listener{std::make_unique<AsyncSocketListener>(cactus::SocketAddress{"127.0.0.1", 0})},
          loop{std::make_unique<EventLoop>()},
          script{std::move(script)} {
    }

    void Close() {
        listener->Close();
    }

    Task<Unit> RunServer() {

        auto client = std::move(co_await listener->Accept());

        std::string buffer;

        for (auto [req, resp] : script) {
            buffer.resize(req.size());
            co_await client->ReadFull(cactus::View(buffer));

            REQUIRE(buffer == req);

            cactus::ConstView resp_view(reinterpret_cast<const std::byte*>(resp.data()),
                                        resp.size());
            co_await client->Write(resp_view);
        }

        co_return Unit{};
    }

    void RunWithClient(std::function<Task<Unit>()> client_func) {
        loop->SetActive();

        bool server_done = false;
        bool client_done = false;
        std::exception_ptr server_error;
        std::exception_ptr client_error;

        auto server_task = [this, &server_done, &server_error]() -> Task<Unit> {
            try {
                co_await RunServer();
                server_done = true;
            } catch (...) {
                server_error = std::current_exception();
                server_done = true;
            }
            co_return Unit{};
        };

        auto client_task = [&client_func, &client_done, &client_error]() -> Task<Unit> {
            try {
                co_await client_func();
                client_done = true;
            } catch (...) {
                client_error = std::current_exception();
                client_done = true;
            }
            co_return Unit{};
        };

        auto server_handle = server_task();
        auto client_handle = client_task();

        server_handle.Resume();
        client_handle.Resume();

        while (!(server_done && client_done)) {
            loop->RunOnce(100);
        }

        loop->Stop();

        if (server_error) {
            std::rethrow_exception(server_error);
        }
        if (client_error) {
            std::rethrow_exception(client_error);
        }
    }

    uint16_t Port() const {
        return listener->Address().GetPort();
    }

    std::unique_ptr<AsyncSocketListener> listener;
    std::unique_ptr<EventLoop> loop;
    Script script;
    std::unique_ptr<Socket> client;
};

}  // namespace

TEST_CASE("Get") {

    Script script = {{"*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n", "$5\r\nvalue\r\n"},
                     {"*2\r\n$3\r\nGET\r\n$7\r\nnot-key\r\n", "$-1\r\n"},
                     {"*2\r\n$3\r\nGET\r\n$4\r\nsome\r\n", "-ERR some error\r\n"}};

    auto server = std::make_shared<MockServer>(std::move(script));
    redis::Client client(cactus::SocketAddress("127.0.0.1", server->Port()));

    auto client_task = [&]() -> Task<Unit> {
        co_await client.Connect();
        auto response = co_await client.Get("key");

        REQUIRE(*response == "value");

        response = co_await client.Get("not-key");

        REQUIRE_FALSE(response.has_value());

        REQUIRE_THROWS_AS(co_await client.Get("some"), RedisError);

        co_await client.Disconnect();

        co_return Unit{};
    };

    server->RunWithClient(client_task);
}

TEST_CASE("GetBadResponse") {
    auto bad_response = GENERATE("aba", ":3\r\nfoo\r\n", "$3\r\nfoo\n\r", "$2\r\nfoo\r\n");
    Script script = {
        {"*2\r\n$3\r\nGET\r\n$4\r\nkey2\r\n", bad_response},
    };

    auto server = std::make_shared<MockServer>(std::move(script));
    redis::Client client(cactus::SocketAddress("127.0.0.1", server->Port()));

    auto client_task = [&]() -> Task<Unit> {
        co_await client.Connect();

        REQUIRE_THROWS_AS(co_await client.Get("key2"), RedisError);

        co_await client.Disconnect();

        co_return Unit{};
    };

    server->RunWithClient(client_task);
}

TEST_CASE("Set") {
    Script script = {{"*3\r\n"
                      "$3\r\nSET\r\n"
                      "$3\r\nkey\r\n"
                      "$5\r\nvalue\r\n",
                      "$2\r\nOK\r\n"},
                     {"*3\r\n"
                      "$3\r\nSET\r\n"
                      "$4\r\nkey1\r\n"
                      "$6\r\nvalue1\r\n",
                      "-ERR Error\r\n"}};

    auto server = std::make_shared<MockServer>(std::move(script));
    redis::Client client(cactus::SocketAddress("127.0.0.1", server->Port()));

    auto client_task = [&]() -> Task<Unit> {
        co_await client.Connect();

        REQUIRE_NOTHROW(co_await client.Set("key", "value"));
        REQUIRE_THROWS_AS(co_await client.Set("key1", "value1"), redis::RedisError);

        co_await client.Disconnect();

        co_return Unit{};
    };

    server->RunWithClient(client_task);
}

TEST_CASE("SetBadResponse") {
    auto bad_response = GENERATE("bar", "$6\r\nNOT_OK\r\n", ":2\r\nOK\r\n", "$2\r\nOK\n\r");
    Script script = {
        {"*3\r\n"
         "$3\r\nSET\r\n"
         "$7\r\nfoo_bar\r\n"
         "$4\r\nabcd\r\n",
         bad_response}};

    auto server = std::make_shared<MockServer>(std::move(script));
    redis::Client client(cactus::SocketAddress("127.0.0.1", server->Port()));

    auto client_task = [&]() -> Task<Unit> {
        co_await client.Connect();

        REQUIRE_THROWS_AS(co_await client.Set("foo_bar", "abcd"), RedisError);

        co_await client.Disconnect();

        co_return Unit{};
    };

    server->RunWithClient(client_task);
}

TEST_CASE("Increment") {
    Script script = {{"*2\r\n"
                      "$4\r\nINCR\r\n"
                      "$3\r\nkey\r\n",
                      ":1\r\n"},
                     {"*2\r\n"
                      "$4\r\nINCR\r\n"
                      "$3\r\nkey\r\n",
                      ":2\r\n"},
                     {"*2\r\n"
                      "$4\r\nINCR\r\n"
                      "$4\r\nkey1\r\n",
                      "-ERR Incr error\r\n"}};

    auto server = std::make_shared<MockServer>(std::move(script));
    redis::Client client(cactus::SocketAddress("127.0.0.1", server->Port()));

    auto client_task = [&]() -> Task<Unit> {
        co_await client.Connect();
        auto response = co_await client.Increment("key");

        REQUIRE(response == 1);

        response = co_await client.Increment("key");

        REQUIRE(response == 2);

        REQUIRE_THROWS_AS(co_await client.Increment("key1"), RedisError);

        co_await client.Disconnect();

        co_return Unit{};
    };

    server->RunWithClient(client_task);
}

TEST_CASE("IncrBadResponse") {
    auto bad_response = GENERATE("caba", "$2\r\nOK\r\n", ":4aba\r\n", ":-8\n\r");
    Script script = {
        {"*2\r\n"
         "$4\r\nINCR\r\n"
         "$5\r\n a b \r\n",
         bad_response}};

    auto server = std::make_shared<MockServer>(std::move(script));
    redis::Client client(cactus::SocketAddress("127.0.0.1", server->Port()));

    auto client_task = [&]() -> Task<Unit> {
        co_await client.Connect();

        REQUIRE_THROWS_AS(co_await client.Increment(" a b "), RedisError);

        co_await client.Disconnect();

        co_return Unit{};
    };

    server->RunWithClient(client_task);
}

TEST_CASE("Decrement") {
    Script script = {{"*2\r\n"
                      "$4\r\nDECR\r\n"
                      "$3\r\nkey\r\n",
                      ":-1\r\n"},
                     {"*2\r\n"
                      "$4\r\nDECR\r\n"
                      "$3\r\nkey\r\n",
                      ":-2\r\n"},
                     {"*2\r\n"
                      "$4\r\nDECR\r\n"
                      "$4\r\nkey1\r\n",
                      "-ERR Another error\r\n"}};

    auto server = std::make_shared<MockServer>(std::move(script));
    redis::Client client(cactus::SocketAddress("127.0.0.1", server->Port()));

    auto client_task = [&]() -> Task<Unit> {
        co_await client.Connect();
        auto response = co_await client.Decrement("key");

        REQUIRE(response == -1);

        response = co_await client.Decrement("key");

        REQUIRE(response == -2);

        REQUIRE_THROWS_AS(co_await client.Decrement("key1"), RedisError);

        co_await client.Disconnect();

        co_return Unit{};
    };

    server->RunWithClient(client_task);
}

TEST_CASE("DecrBadResponse") {
    auto bad_response = GENERATE("caba", "$2\r\nOK\r\n", ":4aba\r\n", ":-8\n\r");
    Script script = {
        {"*2\r\n"
         "$4\r\nDECR\r\n"
         "$4\r\nc  d\r\n",
         bad_response}};

    auto server = std::make_shared<MockServer>(std::move(script));
    redis::Client client(cactus::SocketAddress("127.0.0.1", server->Port()));

    auto client_task = [&]() -> Task<Unit> {
        co_await client.Connect();

        REQUIRE_THROWS_AS(co_await client.Decrement("c  d"), RedisError);

        co_await client.Disconnect();

        co_return Unit{};
    };

    server->RunWithClient(client_task);
}
