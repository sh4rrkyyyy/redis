#include "util.h"

#include <chrono>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <memory>

#include <cactus/rpc/rpc.pb.h>
#include <cactus/rpc/rpc_test_transport.pb.h>

#include <cactus/test.h>

namespace {

using namespace std::chrono_literals;

struct TestServiceImpl : cactus::TestServiceHandler {
    void Search(const cactus::TestRequest& req, cactus::TestResponse* res) override {
        res->set_results(req.query());
    }

    void Ping(const cactus::PingRequest&, cactus::PingResponse*) override {
        throw std::runtime_error{"pong"};
    }
};

struct AnotherServiceImpl : cactus::AnotherServiceHandler {
    int num_pings = 0;

    void Ping(const cactus::PingRequest& req, cactus::PingResponse* rsp) override {
        rsp->set_pong(req.ping() + 1);
        ++num_pings;
    }
};

struct UserServiceImpl : public cactus::UserServiceHandler {
public:
    void Login(const cactus::Credentials& req, cactus::Token* rsp) {
        if ((req.login() == "aba") && (req.password() == "caba")) {
            auto& token = *tokens.emplace(std::to_string(++index_)).first;
            rsp->set_token(token);
        }
    }

    void Logout(const cactus::Token& req, cactus::LogoutResult* rsp) {
        auto ok = (tokens.erase(req.token()) > 0);
        rsp->set_ok(ok);
    }

    void GetAllTokens(const cactus::Token& req, cactus::Tokens* rsp) {
        if (tokens.contains(req.token())) {
            for (const auto& token : tokens) {
                rsp->add_tokens()->set_token(token);
            }
        }
    }

    std::set<std::string> tokens;

private:
    int64_t index_ = 0;
};

void TestSearchRequest(cactus::TestServiceClient* client, std::string_view message) {
    cactus::TestRequest req;
    req.set_query(message.data(), message.size());
    auto rsp = client->Search(req);
    CHECK(rsp.results() == message);
};

void TestPingRequest(cactus::AnotherServiceClient* client, uint32_t ping) {
    cactus::PingRequest req;
    req.set_ping(ping);
    auto rsp = client->Ping(req);
    CHECK(rsp.pong() == ping + 1);
};

class SimpleRpcServerTest {
public:
    template <class T>
    T MakeClient() {
        auto channel = std::make_unique<cactus::SimpleRpcChannel>(server_.GetAddress());
        return T{std::move(channel)};
    }

    void Register(std::unique_ptr<cactus::IRpcService> service) {
        server_.Register(std::move(service));
    }

    template <class Service>
    void Register() {
        Register(std::make_unique<Service>());
    }

    const cactus::SocketAddress& GetAddress() const {
        return server_.GetAddress();
    }

private:
    template <class... Services>
    explicit SimpleRpcServerTest(Services*...) {
        (server_.Register(std::make_unique<Services>()), ...);
        group_.Spawn([this] { server_.Serve(); });
    }

    template <class... Services>
    friend SimpleRpcServerTest MakeTest();

    cactus::SimpleRpcServer server_{{"127.0.0.1", 0}};
    cactus::ServerGroup group_;
};

template <class... Services>
SimpleRpcServerTest MakeTest() {
    return SimpleRpcServerTest{static_cast<Services*>(nullptr)...};
}

}  // namespace

FIBER_TEST_CASE("SimpleCalls") {
    auto test = MakeTest<TestServiceImpl>();
    auto client = test.MakeClient<cactus::TestServiceClient>();

    TestSearchRequest(&client, "test");
    TestSearchRequest(&client, "пластиковые окна");
    TestSearchRequest(&client, "foo bar");

    CHECK_THROWS_AS(client.Ping(cactus::PingRequest{}), cactus::RpcCallError);
}

FIBER_TEST_CASE("UnregisteredService") {
    auto test = MakeTest<>();
    auto client = test.MakeClient<cactus::TestServiceClient>();
    CHECK_THROWS_AS(TestSearchRequest(&client, "aba"), cactus::RpcCallError);
    CHECK_THROWS_AS(client.Ping(cactus::PingRequest{}), cactus::RpcCallError);

    test.Register<TestServiceImpl>();
    TestSearchRequest(&client, "foo");
    TestSearchRequest(&client, "bar");
    CHECK_THROWS_AS(client.Ping(cactus::PingRequest{}), cactus::RpcCallError);
}

FIBER_TEST_CASE("ManyClients") {
    auto test = MakeTest<TestServiceImpl>();
    cactus::WaitGroup group;

    for (auto i : std::views::iota(0, 10)) {
        group.Spawn([&, i] {
            auto client = test.MakeClient<cactus::TestServiceClient>();
            TestSearchRequest(&client, std::to_string(i));
            cactus::SleepFor(100ms);
            TestSearchRequest(&client, std::to_string(1'000 + i));
        });
    }

    CPUTimer timer;
    group.Wait();

    auto times = timer.GetTimes();
    auto wall_time = times.wall_time;

    CHECK(wall_time < 150ms);
    CHECK(wall_time > 100ms);
    CHECK(times.TotalCpuTime() < 50ms);
}

FIBER_TEST_CASE("AnotherService") {
    auto test = MakeTest();
    auto service = std::make_unique<AnotherServiceImpl>();
    const auto& num_pings = service->num_pings;
    test.Register(std::move(service));

    auto client = test.MakeClient<cactus::AnotherServiceClient>();
    for (auto i : std::views::iota(1, 15)) {
        TestPingRequest(&client, 10 + i);
        CHECK(num_pings == i);
    }
}

FIBER_TEST_CASE("MultipleServices") {
    constexpr auto kNumClientsForEachService = 10;

    auto test = MakeTest<TestServiceImpl>();
    auto service = std::make_unique<AnotherServiceImpl>();
    const auto& num_pings = service->num_pings;
    test.Register(std::move(service));

    cactus::WaitGroup group;
    for (auto i : std::views::iota(0, kNumClientsForEachService)) {
        group.Spawn([&, i] {
            auto client = test.MakeClient<cactus::TestServiceClient>();
            TestSearchRequest(&client, std::to_string(2 * i));
            cactus::SleepFor(100ms);
            TestSearchRequest(&client, std::to_string(12'345 + i));
        });
        group.Spawn([&, i] {
            auto client = test.MakeClient<cactus::AnotherServiceClient>();
            TestPingRequest(&client, i);
            cactus::SleepFor(100ms);
            TestPingRequest(&client, 1'000 + i);
        });
    }

    CPUTimer timer;
    group.Wait();

    auto times = timer.GetTimes();
    auto wall_time = times.wall_time;

    CHECK(wall_time < 150ms);
    CHECK(wall_time > 100ms);
    CHECK(times.TotalCpuTime() < 60ms);
    CHECK(num_pings == 2 * kNumClientsForEachService);
}

FIBER_TEST_CASE("UserService") {
    auto test = MakeTest<UserServiceImpl, AnotherServiceImpl>();
    auto client = test.MakeClient<cactus::UserServiceClient>();
    auto ping_client = test.MakeClient<cactus::AnotherServiceClient>();

    cactus::Token token;
    token.set_token("42");

    REQUIRE_FALSE(client.Logout(token).ok());

    cactus::Credentials credentials;
    credentials.set_login("foo");
    credentials.set_password("bar");
    token = client.Login(credentials);

    TestPingRequest(&ping_client, 15);
    REQUIRE(token.token().empty());

    credentials.set_login("aba");
    credentials.set_password("caba");
    token = client.Login(credentials);
    REQUIRE(token.token() == "1");

    token = client.Login(credentials);
    REQUIRE(token.token() == "2");

    auto tokens = client.GetAllTokens(token).tokens();
    REQUIRE(tokens.size() == 2);

    const auto& first = tokens.Get(0);
    const auto& second = tokens.Get(1);

    TestPingRequest(&ping_client, 42);
    REQUIRE(first.token() == "1");
    REQUIRE(second.token() == "2");

    REQUIRE(client.Logout(first).ok());
    REQUIRE(client.GetAllTokens(first).tokens().empty());
    REQUIRE_FALSE(client.Logout(first).ok());

    tokens = client.GetAllTokens(second).tokens();
    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens.Get(0).token() == "2");
    TestPingRequest(&ping_client, 10);
}

FIBER_TEST_CASE("BadHeader") {
    auto test = MakeTest<TestServiceImpl>();
    auto conn = cactus::DialTCP(test.GetAddress());

    uint32_t sizes[] = {7, 10};
    const auto& [header_size, body_size] = sizes;

    conn->Write(cactus::View(sizes));
    conn->Write(cactus::View(std::string(header_size, 'a')));
    conn->Write(cactus::View(std::string(body_size, 'b')));

    conn->ReadFull(cactus::View(sizes));
    auto str = conn->ReadAllToString();
    REQUIRE(str.size() == header_size);
    REQUIRE(body_size == 0);

    cactus::ResponseHeader rsp;
    REQUIRE_NOTHROW(rsp.ParseFromString(str));
    CHECK(rsp.has_rpc_error());
}
