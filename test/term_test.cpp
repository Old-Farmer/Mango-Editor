#include "catch2/catch_test_macros.hpp"

#include "term.h"

using namespace mango;

TEST_CASE("test wcwidth") {
    REQUIRE(Terminal::WCWidth('\t') == -1);
    REQUIRE(Terminal::WCWidth('\n') == -1);
    REQUIRE(Terminal::WCWidth(12) == -1);
}

TEST_CASE("test stringwidth") {
    REQUIRE(Terminal::StringWidth("你好12") == 4 + 2);
}