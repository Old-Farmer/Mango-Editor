#include "catch2/catch_test_macros.hpp"
#include "character.h"

using namespace mango;

// TODO: better unicode test

TEST_CASE("test string display width") {
    CHECK(StringWidth("你好12") == 4 + 2);
    CHECK(StringWidth(" a é न 🇺🇸 👩‍👩‍👧 🏳️‍🌈 👨‍⚕️ 👩‍🚀 💖 "
                        "z") == 26);
    CHECK(StringWidth("A á ❤️ ☝︎ ✊🏿 👨‍👩‍👧‍👦 👩‍❤️‍💋‍👩 🇨🇳 1️⃣ 🏳️‍🌈 ❤︎‍🔥 🧑‍🍼 ǟ̋") == 33);
}

TEST_CASE("bound class test") {
    int byte_eat;
    Codepoint c;
    REQUIRE(Utf8ToUnicode("a", 1, byte_eat, c) == kOk);
    REQUIRE(utf8proc_get_property(c)->boundclass == UTF8PROC_BOUNDCLASS_OTHER);

    REQUIRE(Utf8ToUnicode("柴", strlen("柴"), byte_eat, c) == kOk);
    REQUIRE(utf8proc_get_property(c)->boundclass == UTF8PROC_BOUNDCLASS_OTHER);
}