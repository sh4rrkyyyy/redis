#include <catch2/catch_test_macros.hpp>

#include <cactus/io/writer.h>
#include <cactus/io/reader.h>
#include <cactus/io/view.h>

using namespace cactus;

TEST_CASE("BufferedReader") {
    std::string input = "foo\nbar\nzog";
    ViewReader r(View(input));

    REQUIRE("foo\n" == r.ReadString('\n'));
    REQUIRE("ba" == r.ReadString('\n', 2));
    REQUIRE("r\n" == r.ReadString('\n'));
    REQUIRE("zog" == r.ReadString('\n'));
    REQUIRE(r.ReadString('\n').empty());
}

TEST_CASE("ConstView") {
    int x[1];
    View(x);
}
