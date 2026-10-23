#include "test_utils.h"

#include <cstring>

using namespace redis;        // NOLINT
using namespace redis::test;  // NOLINT

TEST_CASE_METHOD(ReaderTest, "SimpleString") {
    auto reader = MakeParser("+OK\r\n");
    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::SimpleString);
    REQUIRE(resp.AsString() == "OK");
}

TEST_CASE_METHOD(ReaderTest, "Error") {
    auto reader = MakeParser("-ERR Some error\r\n");
    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Error);
    REQUIRE(resp.AsString() == "ERR Some error");
}

TEST_CASE_METHOD(ReaderTest, "Int") {
    auto reader = MakeParser(":42\r\n");
    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Int);
    REQUIRE(resp.AsInteger() == 42);
}

TEST_CASE_METHOD(ReaderTest, "BulkString") {
    auto reader = MakeParser("$11\r\nBulk string\r\n");

    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::BulkString);
    REQUIRE(resp.AsString() == "Bulk string");
}

TEST_CASE_METHOD(ReaderTest, "BulkStringWithNewLine") {
    auto reader = MakeParser("$5\r\nab\r\na\r\n");

    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::BulkString);
    REQUIRE(resp.AsString() == "ab\r\na");
}

TEST_CASE_METHOD(ReaderTest, "NullBulkString") {
    auto reader = MakeParser("$-1\r\n");

    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Empty);
}

TEST_CASE_METHOD(ReaderTest, "Array") {
    auto reader = MakeParser(
        "*2\r\n"
        ":173\r\n"
        "$3\r\nYES\r\n");

    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Array);
    REQUIRE(resp.ArraySize() == 2uz);
    REQUIRE(resp.AsArray()[0]->Type() == ERespType::Int);
    REQUIRE(resp.AsArray()[0]->AsInteger() == 173);
    REQUIRE(resp.AsArray()[1]->Type() == ERespType::BulkString);
    REQUIRE(resp.AsArray()[1]->AsString() == "YES");
}

TEST_CASE_METHOD(ReaderTest, "NullArray") {
    auto reader = MakeParser("*-1\r\n");

    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Empty);
}

TEST_CASE_METHOD(ReaderTest, "ArrayInsideArray") {
    auto reader = MakeParser(
        "*3\r\n"
        "*0\r\n"
        "*3\r\n"
        ":-15\r\n"
        "-WRONG: wrong type\r\n"
        "*-1\r\n"
        "$9\r\nsomething\r\n");

    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Array);
    REQUIRE(resp.ArraySize() == 3uz);
    REQUIRE(resp.AsArray()[0]->Type() == ERespType::Array);
    REQUIRE(resp.AsArray()[0]->arr.empty());
    REQUIRE(resp.AsArray()[1]->Type() == ERespType::Array);
    REQUIRE(resp.AsArray()[1]->arr.size() == 3);
    REQUIRE(resp.AsArray()[1]->arr[0]->Type() == ERespType::Int);
    REQUIRE(resp.AsArray()[1]->arr[0]->AsInteger() == -15);
    REQUIRE(resp.AsArray()[1]->arr[1]->Type() == ERespType::Error);
    REQUIRE(resp.AsArray()[1]->arr[1]->AsString() == "WRONG: wrong type");
    REQUIRE(resp.AsArray()[1]->arr[2]->Type() == ERespType::Empty);
    REQUIRE(resp.AsArray()[2]->Type() == ERespType::BulkString);
    REQUIRE(resp.AsArray()[2]->AsString() == "something");
}

TEST_CASE_METHOD(ReaderTest, "IsStreaming") {
    auto view_reader = MakeViewReader(":-6\r\n");
    SyncReader reader{&view_reader, {}};

    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Int);
    REQUIRE(resp.AsInteger() == -6);

    view_reader = MakeViewReader("+foo\r\n");
    resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::SimpleString);
    REQUIRE(resp.AsString() == "foo");
}

TEST_CASE_METHOD(ReaderTest, "MinMaxInt64") {
    constexpr auto kMin = std::numeric_limits<int64_t>::min();
    constexpr auto kMax = std::numeric_limits<int64_t>::max();

    auto data = ":" + std::to_string(kMin) + "\r\n";
    auto view_reader = MakeViewReader(data.data());
    SyncReader reader{&view_reader, {}};

    auto resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Int);
    REQUIRE(resp.AsInteger() == kMin);

    data = ":" + std::to_string(kMax) + "\r\n";
    view_reader = MakeViewReader(data.data());
    resp = reader.Parse();
    REQUIRE(resp.Type() == ERespType::Int);
    REQUIRE(resp.AsInteger() == kMax);
}

TEST_CASE_METHOD(ReaderTest, "Exceptions") {
    auto check_throws = [this](const char* data) {
        auto reader = MakeParser(data);
        SyncReader sync(reader);
        CHECK_THROWS_AS(sync.Parse(), RedisError);
    };

    check_throws("9223372036854775808\r\n");
    check_throws("4aba\r\n");
    check_throws("15 \r\n");
    check_throws(" -22\r\n");

    check_throws("+aba\rcaba\r\n");
    check_throws("+aba\r\r");
    check_throws("+aba\ncaba\r\n");
    check_throws("+aba\n\n");

    check_throws("*-2\r\n");
    check_throws("$-2\r\n");

    check_throws(":1");
    check_throws("&");
    check_throws("+aboba\r");
}
