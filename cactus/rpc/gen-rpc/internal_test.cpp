#include <set>
#include <string>
#include <memory>

#include <cactus/rpc/rpc_test_internal.pb.h>

#include <catch2/catch_test_macros.hpp>

namespace {

struct PrivateServiceImpl : public ficus::PrivateServiceHandler {
    void BarToBaz(const ficus::Bar& req, ficus::Baz* rsp) override {
        rsp->set_c(req.a() > 9);
        rsp->set_d(req.b() % 1'000);
    }

    void BazToBar(const ficus::Baz& req, ficus::Bar* rsp) override {
        rsp->set_a(10 * req.c());
        rsp->set_b(req.d() % 100);
    }

    void BarToBar(const ficus::Bar& req, ficus::Bar* rsp) override {
        *rsp = req;
    }

    void BazToBaz(const ficus::Baz& req, ficus::Baz* rsp) override {
        *rsp = req;
    }
};

bool operator==(const ficus::Bar& x, const ficus::Bar& y) {
    return (x.a() == y.a()) && (x.b() == y.b());
}

bool operator==(const ficus::Baz& x, const ficus::Baz& y) {
    return (x.c() == y.c()) && (x.d() == y.d());
}

struct UserServiceImpl : public ficus::UserServiceHandler {
public:
    void Login(const ficus::Credentials& req, ficus::Token* rsp) {
        if ((req.login() == "aba") && (req.password() == "caba")) {
            auto& token = *tokens.emplace(std::to_string(++index_)).first;
            rsp->set_token(token);
        }
    }

    void Logout(const ficus::Token& req, ficus::LogoutResult* rsp) {
        auto ok = (tokens.erase(req.token()) > 0);
        rsp->set_ok(ok);
    }

    void GetAllTokens(const ficus::Token& req, ficus::Tokens* rsp) {
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

}  // namespace

TEST_CASE("PrivateService") {
    ficus::PrivateServiceClient client{std::make_unique<PrivateServiceImpl>()};

    {
        ficus::Bar bar;
        bar.set_a(5);
        bar.set_b(12'345);
        auto baz = client.BarToBaz(bar);
        REQUIRE_FALSE(baz.c());
        REQUIRE(baz.d() == 345);
        REQUIRE((client.BarToBar(bar) == bar));
    }

    {
        ficus::Bar bar;
        bar.set_a(10);
        bar.set_b(54'675);
        auto baz = client.BarToBaz(bar);
        REQUIRE(baz.c());
        REQUIRE(baz.d() == 675);
        REQUIRE((client.BarToBar(bar) == bar));
    }

    {
        ficus::Baz baz;
        baz.set_c(false);
        baz.set_d(115);
        auto bar = client.BazToBar(baz);
        REQUIRE(bar.a() == 0);
        REQUIRE(bar.b() == 15);
        REQUIRE((client.BazToBaz(baz) == baz));
    }

    {
        ficus::Baz baz;
        baz.set_c(true);
        baz.set_d(1'213);
        auto bar = client.BazToBar(baz);
        REQUIRE(bar.a() == 10);
        REQUIRE(bar.b() == 13);
        REQUIRE((client.BazToBaz(baz) == baz));
    }
}

TEST_CASE("UserService") {
    ficus::UserServiceClient client{std::make_unique<UserServiceImpl>()};

    ficus::Token token;
    token.set_token("42");

    REQUIRE_FALSE(client.Logout(token).ok());

    ficus::Credentials credentials;
    credentials.set_login("foo");
    credentials.set_password("bar");
    token = client.Login(credentials);

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

    REQUIRE(first.token() == "1");
    REQUIRE(second.token() == "2");

    REQUIRE(client.Logout(first).ok());
    REQUIRE(client.GetAllTokens(first).tokens().empty());
    REQUIRE_FALSE(client.Logout(first).ok());

    tokens = client.GetAllTokens(second).tokens();
    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens.Get(0).token() == "2");
}

TEST_CASE("UserServiceDescriptors") {
    const auto* descriptor = UserServiceImpl{}.ServiceDescriptor();
    REQUIRE(descriptor->method_count() == 3);
    REQUIRE(descriptor->name() == "UserService");
    REQUIRE(descriptor->full_name() == "ficus.UserService");

    const auto* method = descriptor->method(1);
    REQUIRE(method->name() == "Logout");
    REQUIRE(method->full_name() == "ficus.UserService.Logout");

    const auto* input = method->input_type();
    REQUIRE(input->name() == "Token");
    REQUIRE(input->full_name() == "ficus.Token");

    const auto* output = method->output_type();
    REQUIRE(output->name() == "LogoutResult");
    REQUIRE(output->full_name() == "ficus.LogoutResult");
}
