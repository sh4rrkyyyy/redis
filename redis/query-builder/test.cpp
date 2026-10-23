#include "query_builder.h"

#include <ranges>
#include <vector>
#include <list>

#include <catch2/catch_test_macros.hpp>

using namespace redis;  // NOLINT

namespace {
struct TestBuilder {
    QueryBuilder builder;

    void Check(std::string_view expected) const {
        CHECK(builder.Request() == expected);
    }
};
}  // namespace

TEST_CASE_METHOD(TestBuilder, "SimpleString") {
    builder.WriteSimpleString("Simple string");
    Check("+Simple string\r\n");
}

TEST_CASE_METHOD(TestBuilder, "Error") {
    builder.WriteError("ERR Some error");
    Check("-ERR Some error\r\n");
}

TEST_CASE_METHOD(TestBuilder, "Int") {
    builder.WriteInt(-1099);
    Check(":-1099\r\n");
}

TEST_CASE_METHOD(TestBuilder, "BulkString") {
    builder.WriteBulkString("Bulk\r\nstring");
    Check("$12\r\nBulk\r\nstring\r\n");
}

TEST_CASE_METHOD(TestBuilder, "NullBulkString") {
    builder.WriteNullBulkString();
    Check("$-1\r\n");
}

TEST_CASE_METHOD(TestBuilder, "Array") {
    builder.StartArray(3);
    builder.WriteSimpleString("blah");
    builder.WriteInt(90);
    builder.WriteError("ERR 404");
    Check(
        "*3\r\n"
        "+blah\r\n"
        ":90\r\n"
        "-ERR 404\r\n");
}

TEST_CASE_METHOD(TestBuilder, "NullArray") {
    builder.WriteNullArray();
    Check("*-1\r\n");
}

TEST_CASE_METHOD(TestBuilder, "ArrayOfInts") {
    SECTION("vector") {
        builder.WriteArrayInts(std::vector{1, 2, 3});
        Check("*3\r\n:1\r\n:2\r\n:3\r\n");
    }

    SECTION("array") {
        using A = int[];
        builder.WriteArrayInts(A{4, 5});
        Check("*2\r\n:4\r\n:5\r\n");
    }

    SECTION("list") {
        std::list list = {-3, 0};
        builder.WriteArrayInts(list);
        Check("*2\r\n:-3\r\n:0\r\n");
    }

    SECTION("filter") {
        auto f = [](int x) { return x % 2; };
        auto view = std::list{1, 2, 3, 3, 0, 4, 1} | std::views::filter(f);
        builder.WriteArrayInts(view);
        Check("*4\r\n:1\r\n:3\r\n:3\r\n:1\r\n");
    }

    SECTION("empty") {
        builder.WriteArrayInts(std::vector<int64_t>{});
        Check("*0\r\n");
    }
}
