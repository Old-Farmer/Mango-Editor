#include "json.h"

#include "catch2/catch_test_macros.hpp"

using namespace mango;

TEST_CASE("json merge behavior") {
    Json a = Json::parse(R"(
    {
        "color": "red",
        "active": true,
        "name": {"de": "Maus", "en": "mouse"}
    }
    )");
    Json b = Json::parse(R"(
    {
        "color": "blue",
        "active": true,
        "name": {"de": "hello"}
    }
    )");

    a.update(b, true);
    REQUIRE(a["name"]["de"] == "hello");
}