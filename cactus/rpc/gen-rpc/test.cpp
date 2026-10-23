#include <cactus/rpc/rpc_test_gen_rpc.pb.h>

#include <type_traits>
#include <memory>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace {

struct TestChannel : cactus::IRpcChannel {
    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    const google::protobuf::Message& request,
                    google::protobuf::Message* response) override {
        REQUIRE(method->name() == "Search");

        const auto& req = dynamic_cast<const cactus::TestRequest&>(request);
        auto& rsp = dynamic_cast<cactus::TestResponse&>(*response);

        rsp.set_results(req.query());
    }
};

struct TestServiceImpl : cactus::TestServiceHandler {
    int searched = 0;
    int pinged = 0;

    void Search(const cactus::TestRequest& req, cactus::TestResponse* rsp) override {
        rsp->set_results(req.query() + std::to_string(++searched));
    }

    void Ping(const cactus::PingRequest&, cactus::PingResponse*) override {
        ++pinged;
    }
};

}  // namespace

TEST_CASE("Messages") {
    cactus::TestRequest req;
    req.set_query("ping");

    cactus::TestResponse rsp;
    rsp.set_results("pong");
}

TEST_CASE("ClientCodegen") {
    using ChannelPtr = std::unique_ptr<cactus::IRpcChannel>;
    STATIC_CHECK_FALSE(std::is_convertible_v<ChannelPtr, cactus::TestServiceClient>);

    cactus::TestServiceClient stub{std::make_unique<TestChannel>()};

    auto test_request = [&](std::string_view message) {
        cactus::TestRequest req;
        req.set_query(message.data(), message.size());
        auto rsp = stub.Search(req);
        CHECK(rsp.results() == message);
    };

    test_request("foobar");
    test_request("пластиковые окна");
}

TEST_CASE("ServerCodegen") {
    auto impl = std::make_unique<TestServiceImpl>();
    const auto& channel = *impl;
    cactus::TestServiceClient client{std::move(impl)};

    auto test_search_request = [&](std::string_view message, std::string_view expected) {
        auto old_pinged = channel.pinged;
        auto old_searched = channel.searched;

        cactus::TestRequest req;
        req.set_query(message.data(), message.size());
        auto rsp = client.Search(req);

        REQUIRE(rsp.results() == expected);
        REQUIRE(channel.pinged == old_pinged);
        REQUIRE(channel.searched == old_searched + 1);
    };

    auto test_ping_request = [&]() {
        auto old_pinged = channel.pinged;
        auto old_searched = channel.searched;

        client.Ping(cactus::PingRequest{});

        REQUIRE(channel.pinged == old_pinged + 1);
        REQUIRE(channel.searched == old_searched);
    };

    test_search_request("hello", "hello1");
    test_search_request("пластиковые окна", "пластиковые окна2");
    test_ping_request();
    test_ping_request();
    test_search_request("world", "world3");
    test_ping_request();
}

TEST_CASE("Descriptors") {
    TestServiceImpl handler;
    const auto* descriptor = handler.ServiceDescriptor();

    REQUIRE(descriptor->name() == "TestService");
    REQUIRE(descriptor->full_name() == "cactus.TestService");

    const auto* method = descriptor->FindMethodByName("Search");
    REQUIRE(method->name() == "Search");
    REQUIRE(method->full_name() == "cactus.TestService.Search");

    // default instance of message.
    auto* factory = google::protobuf::MessageFactory::generated_factory();
    const auto* req_prototype = factory->GetPrototype(method->input_type());

    // new request object.
    std::unique_ptr<google::protobuf::Message> req{req_prototype->New()};
    CHECK(req->GetTypeName() == "cactus.TestRequest");
}
