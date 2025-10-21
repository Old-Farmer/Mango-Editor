#include "catch2/catch_test_macros.hpp"

#include "term.h"
#include "coding.h"

using namespace mango;

TEST_CASE("test wcwidth") {
    REQUIRE(Terminal::WCWidth(kTabChar) == -1);
    REQUIRE(Terminal::WCWidth('\n') == -1);
    REQUIRE(Terminal::WCWidth(12) == -1);
}